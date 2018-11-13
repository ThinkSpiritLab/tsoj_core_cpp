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

extern std::ofstream log_fp;


std::unique_ptr<UpdateJobBase>
make_update_job(int jobType, ojv4::s_id_type sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn)
{
	try {
		/*
		*  jobType  |   cid    |   type
		*  ------------------------------
		*  0        |     0    |  Exercise
		*  0        |    !0    |  Course
		*  !0       |  0 or !0 |  Contest
		*/
		if (jobType == 0) {
			ojv4::c_id_type c_id = 0;
			static boost::format key_name_tmpl("job_info:0:%d");
			kerbal::redis::Operation opt(redisConn);

			try {
				c_id = opt.hget<int>((key_name_tmpl % sid).str(), "cid");
			} catch (const kerbal::redis::RedisNilException &) {
				c_id = 0;
				//NO NOT THROW
			}  catch (const std::exception & e) {
				EXCEPT_FATAL(jobType, sid, log_fp, "Get cid failed!", e);
				throw;
			} catch (...) {
				LOG_FATAL(jobType, sid, log_fp, "Get cid failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
				throw;
			}
			if (c_id != 0) {
				return std::unique_ptr<UpdateJobBase>(new CourseUpdateJob(jobType, sid, c_id, redisConn, std::move(mysqlConn)));
			} else {
				return std::unique_ptr<UpdateJobBase>(new ExerciseUpdateJob(jobType, sid, redisConn, std::move(mysqlConn)));
			}
		} else {
			return std::unique_ptr<UpdateJobBase>(new ContestUpdateJob(jobType, sid, redisConn, std::move(mysqlConn)));
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Error occurred while construct update job!", e);
		throw;
	}
}

UpdateJobBase::UpdateJobBase(int jobType, ojv4::s_id_type sid, const RedisContext & redisConn,
								std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
			supper_t(jobType, sid, redisConn), mysqlConn(std::move(mysqlConn))
{

	using namespace kerbal::redis;

	static boost::format key_name_tmpl("job_info:%d:%d");
	const std::string key_name = (key_name_tmpl % jobType % sid).str();

	kerbal::redis::Operation opt(redisConn);

	// 取 job 信息
	try {
		this->u_id = opt.hget<ojv4::u_id_type>(key_name, "uid");
		this->s_posttime = opt.hget(key_name, "post_time");
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
		this->similarity_percentage = opt.hget<int>(judge_status, "similarity_percentage");
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
	using namespace kerbal::redis;

	// Step 1: 在 solution 表中插入本 solution 记录
	std::exception_ptr update_solution_exception;
	try {
		this->update_solution();
	} catch (const std::exception & e) {
		update_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		//DO NOT THROW
	}

	std::exception_ptr update_user_exception;
	std::exception_ptr update_problem_exception;
	std::exception_ptr update_user_problem_exception;
	std::exception_ptr update_user_problem_status_exception;
	std::exception_ptr update_source_code_exception;
	std::exception_ptr update_compile_info_exception;
	if (update_solution_exception == nullptr) {

		if (this->result.judge_result != UnitedJudgeResult::SYSTEM_ERROR) { // ignore system error
			// Step 2: 更新用户的提交数与通过数
			//注意!!! 更新用户提交数通过数的操作必须在更新用户题目完成状态成功之前
			try {
				this->update_user();
			} catch (const std::exception & e) {
				update_user_exception = std::current_exception();
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user failed!", e);
				//DO NOT THROW
			}

			// Step 3: 更新题目的提交数与通过数
			//注意!!! 更新题目提交数通过数的操作必须在更新用户题目完成状态成功之前
			try {
				this->update_problem();
			} catch (const std::exception & e) {
				update_problem_exception = std::current_exception();
				EXCEPT_FATAL(jobType, sid, log_fp, "Update problem failed!", e);
				//DO NOT THROW
			}

			// Step 4: 更新/插入 user_problem 表中该用户对应该题目的解题状态
			try {
				this->update_user_problem();
			} catch (const std::exception & e) {
				update_user_problem_exception = std::current_exception();
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user problem failed!", e);
				//DO NOT THROW
			}

			// Step 4.2: 更新/插入 user_problem 表中该用户对应该题目的解题状态
			try {
				this->update_user_problem_status();
			} catch (const std::exception & e) {
				update_user_problem_status_exception = std::current_exception();
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user problem status failed!", e);
				//DO NOT THROW
			}
		}

		// Step 5: 在 source 表或 contest_source%d 表中留存用户代码
		try {
			RedisReply reply = this->get_source_code();
			this->update_source_code(reply->str);
		} catch (const std::exception & e) {
			update_source_code_exception = std::current_exception();
			EXCEPT_WARNING(jobType, sid, log_fp, "Update source code failed!", e);
			//DO NOT THROW
		}

		// Step 6: 在 compile_info 表或 contest_compile_info%d 表中留存编译错误信息
		if (result.judge_result == UnitedJudgeResult::COMPILE_ERROR) {
			try {
				RedisReply reply = this->get_compile_info();
				this->update_compile_info(reply->str);
			} catch (const std::exception & e) {
				update_compile_info_exception = std::current_exception();
				EXCEPT_WARNING(jobType, sid, log_fp, "Update compile info failed!", e);
				//DO NOT THROW
			}
		}
	} else {
		//如果 update solution 失败, 则跳过 update user
		LOG_WARNING(jobType, sid, log_fp, "Escape update user because update solution failed!");
		//如果 update solution 失败, 则跳过 update problem
		LOG_WARNING(jobType, sid, log_fp, "Escape update problem because update solution failed!");
		//如果 update solution 失败, 则跳过 update user problem
		LOG_WARNING(jobType, sid, log_fp, "Escape update user problem because update solution failed!");
		//如果 update solution 失败, 则跳过 update user problem status
		LOG_WARNING(jobType, sid, log_fp, "Escape update user problem status because update solution failed!");
		//如果 update solution 失败, 则跳过 update source code
		LOG_WARNING(jobType, sid, log_fp, "Escape update source code because update solution failed!");
		//如果 update solution 失败, 则跳过 update compile info
		LOG_WARNING(jobType, sid, log_fp, "Escape update compile info because update solution failed!");
	}

	// Step 7: 若所有表都更新成功，则本次同步到 mysql 的操作成功，
	//         应将 redis 数据库中本 solution 的无用的信息删除。
	if (update_solution_exception == nullptr
			&& update_user_exception == nullptr
			&& update_problem_exception == nullptr
			&& update_user_problem_exception == nullptr
			&& update_user_problem_status_exception == nullptr
			&& update_source_code_exception == nullptr
			&& update_compile_info_exception == nullptr) {

		this->clear_this_jobs_info_in_redis();

	} else {
		LOG_WARNING(jobType, sid, log_fp, "Error occurred while update this job!");
	}

	// Step 8: 清除 redis 中过于久远的 solution 数据, 这些 redis 缓存过于久远以致 Web 不大可能访问的到
	this->clear_previous_jobs_info_in_redis();

	this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);
}

kerbal::redis::RedisReply UpdateJobBase::get_compile_info() const
{
	using namespace kerbal::redis;

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
	using namespace kerbal::redis;

	constexpr int REDIS_SOLUTION_MAX_NUM = 600;

	if (sid > REDIS_SOLUTION_MAX_NUM) {
		int prev_sid = this->sid - REDIS_SOLUTION_MAX_NUM;

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
//					LOG_WARNING(jobType, sid, log_fp, "Doesn't delete judge_status actually!");
				}
			} catch (const std::exception & e) {
				LOG_WARNING(jobType, sid, log_fp, "Exception occurred while deleting judge_status!");
				//DO NOT THROW
			}

			try {
				static boost::format similarity_details("similarity_details:%d:%d");
				if (opt.del((similarity_details % jobType % prev_sid).str()) == false) {
//					LOG_WARNING(jobType, sid, log_fp, "Doesn't delete similarity_details actually!");
				}
			} catch (const std::exception & e) {
				LOG_WARNING(jobType, sid, log_fp, "Exception occurred while deleting similarity_details!");
				//DO NOT THROW
			}

			try {
				static boost::format job_info("job_info:%d:%d");
				if (opt.del((job_info % jobType % prev_sid).str()) == false) {
//					LOG_WARNING(jobType, sid, log_fp, "Doesn't delete job_info actually!");
				}
			} catch (const std::exception & e) {
				LOG_WARNING(jobType, sid, log_fp, "Exception occurred while deleting job_info!");
				//DO NOT THROW
			}

		}
	}
} catch(...) {
	UNKNOWN_EXCEPT_FATAL(jobType, sid, log_fp, "Clear previous jobs info in redis failed!");
	//DO NOT THROW BECAUSE THIS METHOD HAS BEEN DECLEARED AS NOEXCEPT
}

void UpdateJobBase::clear_this_jobs_info_in_redis() noexcept try
{
	using namespace kerbal::redis;

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

void UpdateJobBase::core_update_failed_table(const kerbal::redis::RedisReply & source_code, const kerbal::redis::RedisReply & compile_error_info) noexcept try
{
	LOG_WARNING(jobType, sid, log_fp, "Enter core update failed handler");

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
//		jobType, sid, pid, uid, (int) lang,
//		postTime, cid, cases, (int)timeLimit.count(), (int)memoryLimit.count(),
//		similarity_threshold, (int) result.judge_result, (int)result.cpu_time.count(),
//		(int)result.memory.count(), result.similarity_percentage, source_code->str, compile_error_info->str
//	);
//
//	if (!res) {
//		throw MysqlEmptyResException(insert.errnum(), insert.error());
//	}
} catch(const std::exception & e) {
	EXCEPT_FATAL(jobType, sid, log_fp, "Update failed table failed!", e);
} catch(...) {
	UNKNOWN_EXCEPT_FATAL(jobType, sid, log_fp, "Update failed table failed!");
}
