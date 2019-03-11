/*
 * UpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "UpdateJobBase.hpp"
#include "ExerciseUpdateJob.hpp"
#include "CourseUpdateJob.hpp"
#include "ContestUpdateJob.hpp"
#include "ExerciseRejudgeJob.hpp"
#include "CourseRejudgeJob.hpp"
#include "ContestRejudgeJob.hpp"
#include "logger.hpp"
#include "boost_format_suffix.hpp"

#include <boost/lexical_cast.hpp>
#include <kerbal/data_struct/optional/optional.hpp>
#include <mysql_conn_factory.hpp>

extern std::ofstream log_fp;

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/transaction.h>

#include <functional>


std::unique_ptr<UpdateJobBase>
make_update_job(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn)
{
    LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	using return_type = std::unique_ptr<UpdateJobBase>;
	using kerbal::data_struct::optional;
	using kerbal::data_struct::nullopt;

	thread_local static boost::format key_name_tmpl("job_info:%d:%d");
	kerbal::redis_v2::hash h(redis_conn, (key_name_tmpl % jobType % s_id).str());

	bool is_rejudge = false;
	try {
		is_rejudge = static_cast<bool>(h.hget<optional<int>>("is_rejudge").value_or(0));
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get is_rejudge failed!", e);
		throw;
	} catch (...) {
		UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Get is_rejudge failed!");
		throw;
	}

	/*
	 *  jobType  |   c_id   |   type
	 *  ------------------------------
	 *  0        |     0    |  Exercise
	 *  0        |    !0    |  Course
	 *  !0       |  0 or !0 |  Contest
	 */
	if (jobType == 0) {


		optional<ojv4::c_id_type> c_id(nullopt);
		try {
			c_id = h.hget<optional<ojv4::c_id_type>>("cid");
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "Get c_id failed!", e);
			throw;
		} catch (...) {
			UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Get c_id failed!");
			throw;
		}

		if (c_id != nullopt && *c_id != 0_c_id) { // 取到的课程 id 不为空, 且明确非零情况下, 表明这是一个课程任务, 而不是练习任务

			if (is_rejudge == true) { // 取到的 is_rejudge 字段非空, 且明确为真的情况下, 表明这是一个重评任务
				return return_type(new CourseRejudgeJob(jobType, s_id, *c_id, redis_conn));
			} else {
				return return_type(new CourseUpdateJob(jobType, s_id, *c_id, redis_conn));
			}
		} else {
			if (is_rejudge == true) {
				return return_type(new ExerciseRejudgeJob(jobType, s_id, redis_conn));
			} else {
				return return_type(new ExerciseUpdateJob(jobType, s_id, redis_conn));
			}
		}
	} else {
		if (is_rejudge == true) {
			return return_type(new ContestRejudgeJob(jobType, s_id, redis_conn));
		} else {
			return return_type(new ContestUpdateJob(jobType, s_id, redis_conn));
		}
	}
}

UpdateJobBase::UpdateJobBase(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		JobBase(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	using boost::lexical_cast;
	using namespace kerbal::utility;
	namespace optional_ns = kerbal::data_struct;
	using namespace optional_ns;

	{
		// 取 job 信息
		thread_local static boost::format key_name_tmpl("job_info:%d:%d");
		kerbal::redis_v2::hash h(redis_conn, (key_name_tmpl % jobType % s_id).str());
		std::vector<optional<std::string>> job_info = h.hmget(
				"uid",
				"post_time");

		this->u_id = lexical_cast<ojv4::u_id_type>(job_info[0].value());
		this->s_posttime = std::move(job_info[1].value());
	}

	{
		// 取评测结果
		thread_local static boost::format judge_status_name_tmpl("judge_status:%d:%d");
		kerbal::redis_v2::hash h(redis_conn, (judge_status_name_tmpl % jobType % s_id).str());
		std::vector<optional<std::string>> judge_status = h.hmget(
				"result",
				"cpu_time",
				"real_time",
				"memory",
				"similarity_percentage");

		this->result.judge_result = ojv4::s_result_enum(lexical_cast<int>(judge_status[0].value()));
		this->result.cpu_time = ojv4::s_time_in_milliseconds(lexical_cast<int>(judge_status[1].value()));
		this->result.real_time = ojv4::s_time_in_milliseconds(lexical_cast<int>(judge_status[2].value()));
		this->result.memory = ojv4::s_mem_in_byte(lexical_cast<int>(judge_status[3].value()));
		this->similarity_percentage = lexical_cast<int>(judge_status[4].value());
	}
}

void UpdateJobBase::handle()
{
	using namespace kerbal::redis;

	std::exception_ptr update_solution_exception = nullptr;
	std::exception_ptr update_source_code_exception = nullptr;
	std::exception_ptr update_compile_info_exception = nullptr;
	std::exception_ptr update_user_exception = nullptr;
	std::exception_ptr update_problem_exception = nullptr;
	std::exception_ptr update_user_problem_exception = nullptr;
	std::exception_ptr update_user_problem_status_exception = nullptr;
	std::exception_ptr commit_exception = nullptr;
	std::reference_wrapper<const std::exception_ptr> exception_ptrs[8] = {
		update_solution_exception,
		update_source_code_exception,
		update_compile_info_exception,
		update_user_exception,
		update_problem_exception,
		update_user_problem_exception,
		update_user_problem_status_exception,
		commit_exception
	};

	// 本次更新任务的 mysql 连接
	auto mysql_conn_handle = sync_fetch_mysql_conn();
	mysqlpp::Connection & mysql_conn = *mysql_conn_handle;

	mysqlpp::Transaction trans(mysql_conn);

	// Step 1: 在 solution 表中插入本 solution 记录
	try {
		this->update_solution(mysql_conn);
	} catch (const std::exception & e) {
		update_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		//DO NOT THROW
	}

	if (update_solution_exception == nullptr) {

		// Step 2: 在 source 表或 contest_source%d 表中留存用户代码
		try {
			this->update_source_code(mysql_conn);
		} catch (const std::exception & e) {
			update_source_code_exception = std::current_exception();
			EXCEPT_WARNING(jobType, s_id, log_fp, "Update source code failed!", e);
			//DO NOT THROW
		}

		// Step 3: 在 compile_info 表或 contest_compile_info%d 表中留存编译错误信息
		if (result.judge_result == UnitedJudgeResult::COMPILE_ERROR) {
			try {
				this->update_compile_info(mysql_conn);
			} catch (const std::exception & e) {
				update_compile_info_exception = std::current_exception();
				EXCEPT_WARNING(jobType, s_id, log_fp, "Update compile info failed!", e);
				//DO NOT THROW
			}
		}

		// Step 4: 更新用户的提交数与通过数
		//注意!!! 更新用户提交数通过数的操作必须在更新用户题目完成状态成功之前
		try {
			this->update_user(mysql_conn);
		} catch (const std::exception & e) {
			update_user_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user failed!", e);
			//DO NOT THROW
		}

		// Step 5: 更新题目的提交数与通过数
		//注意!!! 更新题目提交数通过数的操作必须在更新用户题目完成状态成功之前
		try {
			this->update_problem(mysql_conn);
		} catch (const std::exception & e) {
			update_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem failed!", e);
			//DO NOT THROW
		}

		// Step 6: 更新/插入 user_problem 表中该用户对应该题目的解题状态
		try {
			this->update_user_problem(mysql_conn);
		} catch (const std::exception & e) {
			update_user_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
			//DO NOT THROW
		}

//		// Step 6.2: 更新/插入 user_problem 表中该用户对应该题目的解题状态
//		try {
//			this->update_user_problem_status(mysql_conn);
//		} catch (const std::exception & e) {
//			update_user_problem_status_exception = std::current_exception();
//			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem status failed!", e);
//			//DO NOT THROW
//		}

		try {
			trans.commit();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "MySQL commit failed!", e);
			commit_exception = std::current_exception();
			//DO NOT THROW
		}
	}

	auto is_nullptr = [](const std::exception_ptr & ep) noexcept {
		return ep == nullptr;
	};

	// Step 7: 若所有表都更新成功，则本次同步到 mysql 的操作成功，
	//         应将 redis 数据库中本 solution 的无用的信息删除。
	if (std::all_of(std::begin(exception_ptrs), std::end(exception_ptrs), is_nullptr) == true) {
		this->clear_this_jobs_info_in_redis();
	} else {
		LOG_WARNING(jobType, s_id, log_fp, "Error occurred while update this job!");
	}

	this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);

	// Step 8: 清除 redis 中过于久远的 solution 数据, 这些 redis 缓存过于久远以致 Web 不大可能访问的到
	//this->clear_previous_jobs_info_in_redis();

}

#include "db_typedef.hpp"

kerbal::redis_v2::reply UpdateJobBase::get_compile_info() const
{
    auto redis_conn_handler = sync_fetch_redis_conn();
	kerbal::redis_v2::reply reply = redis_conn_handler->execute("get compile_info:%d:%d", this->jobType, this->s_id.to_literal());

	if (reply.type() != kerbal::redis_v2::reply_type::STRING) {
		throw std::runtime_error("Wrong compile info type. Expected: STRING, actually get: " + reply_type_name(reply.type()));
	}
	return reply;
}

void UpdateJobBase::clear_previous_jobs_info_in_redis() noexcept try
{
	constexpr int REDIS_SOLUTION_MAX_NUM = 600;

	if (s_id.to_literal() <= REDIS_SOLUTION_MAX_NUM) {
		return;
	}
	int prev_s_id = this->s_id.to_literal() - REDIS_SOLUTION_MAX_NUM;

    auto redis_conn_handler = sync_fetch_redis_conn();
	kerbal::redis_v2::connection & redis_conn = *redis_conn_handler;
	boost::format judge_status_templ("judge_status:%d:%d");

	try {
		JudgeStatus judge_status = JudgeStatus::UPDATED;
		kerbal::redis_v2::hash h(redis_conn, (judge_status_templ % jobType % prev_s_id).str());
		judge_status = (JudgeStatus) h.hget<int>("status");
		if (judge_status != JudgeStatus::UPDATED) {
			return;
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get judge status failed!", e);
		return; //DO NOT THROW
	}

	kerbal::redis_v2::operation opt(redis_conn);
	try {
		if (opt.del((judge_status_templ % jobType % prev_s_id).str()) == false) {
			LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete judge_status actually!");
		}
	} catch (const std::exception & e) {
		LOG_WARNING(jobType, s_id, log_fp, "Exception occurred while deleting judge_status!");
		//DO NOT THROW
	}

	try {
		boost::format similarity_details("similarity_details:%d:%d");
		if (opt.del((similarity_details % jobType % prev_s_id).str()) == false) {
			//LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete similarity_details actually!");
		}
	} catch (const std::exception & e) {
		LOG_WARNING(jobType, s_id, log_fp, "Exception occurred while deleting similarity_details!");
		//DO NOT THROW
	}

	try {
		boost::format job_info("job_info:%d:%d");
		if (opt.del((job_info % jobType % prev_s_id).str()) == false) {
			LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete job_info actually!");
		}
	} catch (const std::exception & e) {
		LOG_WARNING(jobType, s_id, log_fp, "Exception occurred while deleting job_info!");
		//DO NOT THROW
	}
} catch(...) {
	UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Clear previous jobs info in redis failed!");
	//DO NOT THROW BECAUSE THIS METHOD HAS BEEN DECLEARED AS NOEXCEPT
}

void UpdateJobBase::clear_this_jobs_info_in_redis() noexcept try
{
	using namespace kerbal::redis;

    auto redis_conn_handler = sync_fetch_redis_conn();
	kerbal::redis_v2::connection & redis_conn = *redis_conn_handler;
	kerbal::redis_v2::operation opt(redis_conn);
	try {
		boost::format source_code("source_code:%d:%d");
		if (opt.del((source_code % jobType % s_id).str()) == false) {
			LOG_WARNING(jobType, s_id, log_fp, "Source code doesn't exist!");
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Exception occurred while deleting source code!", e);
		//DO NOT THROW
	}

	try {
		boost::format compile_info("compile_info:%d:%d");
		opt.del((compile_info % jobType % s_id).str());
		// 可能 job 编译通过没有 compile_info, 所以不管有没有都 del 一次, 不过不用判断是否删除成功
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Exception occurred while deleting compile info!", e);
		//DO NOT THROW
	}
} catch (...) {
	UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Clear this jobs info in redis failed!");
}

void UpdateJobBase::core_update_failed_table(const kerbal::redis::RedisReply & source_code, const kerbal::redis::RedisReply & compile_error_info) noexcept try
{
	LOG_WARNING(jobType, s_id, log_fp, "Enter core update failed handler");

//	mysqlpp::Query insert = mysqlConn->query(
//			"insert into core_update_failed "
//			"(type, job_id, pid, uid, lang, "
//			"post_time, cid, cases, time_limit, memory_limit, "
//			"sim_threshold, result, cpu_time, memory, sim_percentage, source_code, compile_error_info) "
//			"values (%0, %1, %2, %3, %4, "
//			"%5q, %6, %7, %8, %9, "
//			"%10, %11, %12, %13, %14, %15q, %16q)"
//	);
//	insert.parse();
//
//	mysqlpp::SimpleResult res = insert.execute(
//		jobType, s_id, pid, uid, (int) lang,
//		postTime, cid, cases, (int)timeLimit.count(), (int)memoryLimit.count(),
//		similarity_threshold, (int) result.judge_result, (int)result.cpu_time.count(),
//		(int)result.memory.count(), result.similarity_percentage, source_code->str, compile_error_info->str
//	);
//
//	if (!res) {
//		throw MysqlEmptyResException(insert.errnum(), insert.error());
//	}
} catch(const std::exception & e) {
	EXCEPT_FATAL(jobType, s_id, log_fp, "Update failed table failed!", e);
}
