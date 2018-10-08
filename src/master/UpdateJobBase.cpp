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
#include "logger.hpp"
#include "boost_format_suffix.hpp"

extern std::ostream log_fp;

namespace
{
	using namespace kerbal::redis;
}


std::unique_ptr<UpdateJobBase>
make_update_job(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn)
{
	UpdateJobBase * job = nullptr;

	try {
		/*
		*  jobType  |   cid    |   type
		*  ------------------------------
		*  0        |     0    |  Exercise
		*  0        |    !0    |  Course
		*  !0       |  0 or !0 |  Contest
		*/
		if (jobType == 0) {
			/** course_id */
			int cid = 0;
			static boost::format key_name_tmpl("job_info:0:%d");
			kerbal::redis::Operation opt(redisConn);

			try {
				cid = opt.hget<int>((key_name_tmpl % sid).str(), "cid");
			} catch (const std::exception & e) {
				EXCEPT_FATAL(jobType, sid, log_fp, "Get cid failed!", e);
				throw;
			} catch (...) {
				LOG_FATAL(jobType, sid, log_fp, "Get cid failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
				throw;
			}
			if (cid != 0) {
				job = new CourseUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
			} else {
				job = new ExerciseUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
			}
		} else {
			job = new ContestUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Error occurred while construct update job!", e);
		throw;
	} catch (...) {
		LOG_FATAL(jobType, sid, log_fp, "Error occurred while construct update job! Error information: ", UNKNOWN_EXCEPTION_WHAT);
		throw;
	}

	// 此处返回的是指向派生类的基类指针，需小心使用
	return std::unique_ptr<UpdateJobBase>(job);
}

UpdateJobBase::UpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
								std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
			supper_t(jobType, sid, redisConn), mysqlConn(std::move(mysqlConn))
{
	static boost::format key_name_tmpl("job_info:%d:%d");
	const std::string key_name = (key_name_tmpl % jobType % sid).str();

	kerbal::redis::Operation opt(redisConn);

	// 取 job 信息
	try {
		this->uid = opt.hget<int>(key_name, "uid");
		this->cid = opt.hget<int>(key_name, "cid");
		this->postTime = opt.hget(key_name, "post_time");
		//this->haveAccepted = (bool) opt.hget<int>(key_name, "have_accepted");
		this->no_store_ac_code = (bool) opt.hget<int>(key_name, "no_store_ac_code");
		this->is_rejudge = (bool) opt.hget<int>(key_name, "is_rejudge");
	} catch (const RedisNilException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "job details lost.", e);
		throw;
	} catch (const RedisUnexpectedCaseException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "redis returns an unexpected type.", e);
		throw;
	} catch (const RedisException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	}

	static boost::format judge_status_name_tmpl("judge_status:%d:%d");
	const std::string judge_status = (judge_status_name_tmpl % jobType % sid).str();

	// 取评测结果
	try {
		this->result.judge_result = (UnitedJudgeResult) opt.hget<int>(judge_status, "result");
		this->result.cpu_time = std::chrono::milliseconds(opt.hget<int>(judge_status, "cpu_time"));
		this->result.real_time = std::chrono::milliseconds(opt.hget<int>(judge_status, "real_time"));
		this->result.memory = kerbal::utility::Byte(opt.hget<int>(judge_status, "memory"));
		this->result.similarity_percentage = opt.hget<int>(judge_status, "similarity_percentage");
	} catch (const RedisNilException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "job details lost.", e);
		throw;
	} catch (const RedisUnexpectedCaseException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "redis returns an unexpected type.", e);
		throw;
	} catch (const RedisException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	}
}

void UpdateJobBase::handle()
{
	// Step 1: 在 solution 表中插入本 solution 记录
	std::exception_ptr update_solution_exception;
	try {
		this->update_solution();
	} catch (const std::exception & e) {
		update_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		//DO NOT THROW
	}

	// Step 2: 更新 problem 表与 user 表中的提交数与通过数
	std::exception_ptr update_user_and_problem_exception;
	if(update_solution_exception == nullptr) {
		//如果 update solution 失败, 则跳过 update user and problem
		//注意!!! 更新用户提交数通过数, 题目提交数通过数的操作必须在更新用户题目完成状态成功之前
		try {
			this->update_user_and_problem();
		} catch (const std::exception & e) {
			update_user_and_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, sid, log_fp, "Update user and problem failed!", e);
			//DO NOT THROW
		}
	} else {
		LOG_WARNING(jobType, sid, log_fp, "Escape update user and problem because update solution failed!");
	}

	// Step 3: 更新/插入 user_problem 表中该用户对应该题目的解题状态
	std::exception_ptr update_user_problem_exception;
	if(update_solution_exception == nullptr) { //如果 update solution 失败, 则跳过 update user problem
		try {
			this->update_user_problem();
		} catch (const std::exception & e) {
			update_user_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, sid, log_fp, "Update user problem failed!", e);
			//DO NOT THROW
		}
	} else {
		LOG_WARNING(jobType, sid, log_fp, "Escape update user problem because update solution failed!");
	}

	// Step 4: 在 source 表或 contest_source* 表中存储 s_id 对应的题解内容
	std::exception_ptr update_source_code_exception;
	if(update_solution_exception == nullptr) { //如果 update solution 失败, 则跳过 update source code
		try
		{ // update source code
			// get_source_code() 来源于 JobBase
			RedisReply reply = this->get_source_code();
			this->update_source_code(reply->str);
		} catch (const std::exception & e) {
			update_source_code_exception = std::current_exception();
			EXCEPT_WARNING(jobType, sid, log_fp, "Update source code failed!", e);
			//DO NOT THROW
		}
	} else {
		LOG_WARNING(jobType, sid, log_fp, "Escape update source code because update solution failed!");
	}

	std::exception_ptr update_compile_info_exception;
	// Step 5: 在 compile_info 表或 contest_compile_info* 表中插入s_id 对应的编译错误信息
	if (result.judge_result == UnitedJudgeResult::COMPILE_ERROR) {
		if(update_solution_exception == nullptr) { //如果 update solution 失败, 则跳过 update compile info
			try {
				RedisReply reply = this->get_compile_info();
				this->update_compile_info(reply->str);
			} catch (const std::exception & e) {
				update_compile_info_exception = std::current_exception();
				EXCEPT_WARNING(jobType, sid, log_fp, "Update compile info failed!", e);
				//DO NOT THROW
			}
		} else {
			LOG_WARNING(jobType, sid, log_fp, "Escape update compile info because update solution failed!");
		}
	}

	Operation opt(redisConn);

	// Step 6: 若所有表都更新成功，则本次同步到 mysql 的操作成功，
	//         应将此状态反馈到 redis 数据库，并将 redis 数据库中本 solution 的无用的信息删除。
	if (update_solution_exception == nullptr
			&& update_user_and_problem_exception == nullptr
			&& update_user_problem_exception == nullptr
			&& update_source_code_exception == nullptr
			&& update_compile_info_exception == nullptr) {
		this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);

		this->clear_this_jobs_info_in_redis();

	} else {
		LOG_WARNING(jobType, sid, log_fp, "Error occurred while update this job!");
	}

	// Step 7: 清除 redis 中过于久远的 solution 数据 (mysql 中有留存)，防止内存爆炸
	//         可以理解为有延迟的将内存数据永久存储在硬盘中
	this->clear_previous_jobs_info_in_redis();
}

kerbal::redis::RedisReply UpdateJobBase::get_compile_info() const
{
	static RedisCommand query("get compile_info:%d:%d");
	RedisReply reply;
	try {
		reply = query.execute(redisConn, this->jobType, this->sid);
	} catch (std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Get compile info failed!", e);
		throw;
	}
	if (reply.replyType() != RedisReplyType::STRING) {
		RedisUnexpectedCaseException e(reply.replyType());
		EXCEPT_FATAL(jobType, sid, log_fp, "Get compile info failed!", e);
		throw e;
	}
	return reply;
}

void UpdateJobBase::clear_previous_jobs_info_in_redis() noexcept try
{
	// del solution - 600

	if (sid > 600) {
		int prev_sid = this->sid - 600;

		Operation opt(this->redisConn);

		static boost::format judge_status_templ("judge_status:%d:%d");

		JudgeStatus judge_status = JudgeStatus::UPDATED;
		try {
			judge_status = (JudgeStatus) opt.hget<int>((judge_status_templ % jobType % prev_sid).str(), "status");
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, sid, log_fp, "Get judge status failed!", e);
			return; //DO NOT THROW
		}

		if (judge_status == JudgeStatus::UPDATED) {
			try {
				if (opt.del((judge_status_templ % jobType % prev_sid).str()) == false) {
					LOG_WARNING(jobType, sid, log_fp, "Doesn't delete judge_status actually!");
				}
			} catch (const std::exception & e) {
				LOG_WARNING(jobType, sid, log_fp, "Exception occurred while deleting judge_status!");
				//DO NOT THROW
			}

			try {
				static boost::format job_info("job_info:%d:%d");
				if (opt.del((job_info % jobType % prev_sid).str()) == false) {
					LOG_WARNING(jobType, sid, log_fp, "Doesn't delete job_info actually!");
				}
			} catch (const std::exception & e) {
				LOG_WARNING(jobType, sid, log_fp, "Exception occurred while deleting job_info!");
				//DO NOT THROW
			}
		}
	}
} catch(...) {
	LOG_FATAL(jobType, sid, log_fp, "Clear previous jobs info in redis failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
}

void UpdateJobBase::clear_this_jobs_info_in_redis() noexcept try
{
	Operation opt(this->redisConn);
	try {
		static boost::format source_code("source_code:%d:%d");
		if (opt.del((source_code % jobType % sid).str()) == false) {
			LOG_WARNING(jobType, sid, log_fp, "Source code doesn't exist!");
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Exception occurred while deleting source code!", e);
		//DO NOT THROW
	}

	try {
		static boost::format compile_info("compile_info:%d:%d");
		opt.del((compile_info % jobType % sid).str());
		// 可能 job 编译通过没有 compile_info, 所以不管有没有都 del 一次, 不过不用判断是否删除成功
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Exception occurred while deleting compile info!", e);
		//DO NOT THROW
	}
} catch (...) {
	LOG_FATAL(jobType, sid, log_fp, "Clear this jobs info in redis failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
}

void UpdateJobBase::core_update_failed_table(const RedisReply & source_code, const RedisReply & compile_error_info) noexcept try
{
	LOG_WARNING(jobType, sid, log_fp, "Enter core update failed handler");

	mysqlpp::Query insert = mysqlConn->query(
			"insert into core_update_failed "
			"(type, job_id, pid, uid, lang, "
			"post_time, cid, cases, time_limit, memory_limit, "
			"sim_threshold, result, cpu_time, memory, sim_percentage, source_code, compile_error_info) "
			"values (%0, %1, %2, %3, %4, "
			"%5q, %6, %7, %8, %9, "
			"%10, %11, %12, %13, %14, %15q, %16q)"
	);
	insert.parse();

	mysqlpp::SimpleResult res = insert.execute(
		jobType, sid, pid, uid, (int) lang,
		postTime, cid, cases, (int)timeLimit.count(), (int)memoryLimit.count(),
		similarity_threshold, (int) result.judge_result, (int)result.cpu_time.count(),
		(int)result.memory.count(), result.similarity_percentage, source_code->str, compile_error_info->str
	);

	if (!res) {
		throw MysqlEmptyResException(insert.errnum(), insert.error());
	}
} catch(const std::exception & e) {
	EXCEPT_FATAL(jobType, sid, log_fp, "Update failed table failed!", e);
} catch(...) {
	LOG_FATAL(jobType, sid, log_fp, "Update failed table failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
}
