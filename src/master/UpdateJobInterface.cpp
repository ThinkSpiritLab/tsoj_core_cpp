/*
 * UpdateJobInterface.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "UpdateJobInterface.hpp"

#include "ExerciseUpdateJob.hpp"
#include "CourseUpdateJob.hpp"
#include "ContestUpdateJob.hpp"
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


std::unique_ptr<ConcreteUpdateJob>
make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	using return_type = std::unique_ptr<ConcreteUpdateJob>;
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

		optional<oj::c_id_type> c_id(nullopt);
		try {
			c_id = h.hget<optional<oj::c_id_type>>("cid");
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "Get c_id failed!", e);
			throw;
		} catch (...) {
			UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Get c_id failed!");
			throw;
		}

		if (c_id != nullopt && *c_id != 0_c_id) { // 取到的课程 id 不为空, 且明确非零情况下, 表明这是一个课程任务, 而不是练习任务
			if (is_rejudge == true) { // 取到的 is_rejudge 字段非空, 且明确为真的情况下, 表明这是一个重评任务
				return return_type(new RejudgeCourseJob(jobType, s_id, *c_id, redis_conn));
			} else {
				return return_type(new BasicCourseJob(jobType, s_id, *c_id, redis_conn));
			}
		} else {
			if (is_rejudge == true) {
				return return_type(new RejudgeExerciseJob(jobType, s_id, redis_conn));
			} else {
				return return_type(new BasicExerciseJob(jobType, s_id, redis_conn));
			}
		}
	} else {
		if (is_rejudge == true) {
			return return_type(new RejudgeContestJob(jobType, s_id, redis_conn));
		} else {
			return return_type(new BasicContestJob(jobType, s_id, redis_conn));
		}
	}
}

UpdateJobInterface::UpdateJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		JobBase(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	using boost::lexical_cast;
	using namespace kerbal::utility;
	namespace optional_ns = kerbal::data_struct;
	using namespace optional_ns;

	// 取 job 信息
	thread_local static boost::format key_name_tmpl("job_info:%d:%d");
	kerbal::redis_v2::hash h(redis_conn, (key_name_tmpl % jobType % s_id).str());
	std::vector<optional<std::string>> job_info = h.hmget(
			"uid",
			"post_time");

	this->u_id = lexical_cast<oj::u_id_type>(job_info[0].value());
	this->s_posttime = std::move(job_info[1].value());
}

ConcreteUpdateJob::ConcreteUpdateJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	using kerbal::data_struct::optional;
	using boost::lexical_cast;

	// 取评测结果
	thread_local static boost::format judge_status_name_tmpl("judge_status:%d:%d");
	kerbal::redis_v2::hash h(redis_conn, (judge_status_name_tmpl % jobType % s_id).str());
	std::vector<optional<std::string>> judge_status = h.hmget(
			"result",
			"cpu_time",
			"real_time",
			"memory",
			"similarity_percentage");

	this->judge_result = oj::s_result_enum(lexical_cast<int>(judge_status[0].value()));
	this->cpu_time = oj::s_time_in_milliseconds(lexical_cast<int>(judge_status[1].value()));
	this->real_time = oj::s_time_in_milliseconds(lexical_cast<int>(judge_status[2].value()));
	this->memory = oj::s_mem_in_byte(lexical_cast<int>(judge_status[3].value()));
	this->similarity_percentage = lexical_cast<int>(judge_status[4].value());
}

kerbal::redis_v2::reply ConcreteUpdateJob::get_compile_info() const
{
	auto redis_conn_handler = sync_fetch_redis_conn();
	kerbal::redis_v2::reply reply = redis_conn_handler->execute("get compile_info:%d:%d", this->jobType, this->s_id.to_literal());

	if (reply.type() != kerbal::redis_v2::reply_type::STRING) {
		throw std::runtime_error("Wrong compile info type. Expected: STRING, actually get: " + reply_type_name(reply.type()));
	}
	return reply;
}

void ConcreteUpdateJob::clear_this_jobs_info_in_redis() noexcept try
{
	try {
		std::initializer_list<std::string> args = {"EVAL", R"===(
			local JUDGE_STATUS__UPDATED = ARGV[3]
			local job_type = ARGV[1]
			local s_id = ARGV[2]
			local delete_result = {0, 0}
			local judge_status = string.format('judge_status:%d:%d', job_type, s_id)
			if (redis.call('hget', judge_status, 'status') == JUDGE_STATUS__UPDATED) then
				delete_result[1] = redis.call('del', string.format('source_code:%d:%d', job_type, s_id))	# delete source_code:%d:%d
				delete_result[2] = redis.call('del', string.format('compile_info:%d:%d', job_type, s_id))	# delete compile_info:%d:%d
			end
			return delete_result
		)===", "0", std::to_string(jobType), std::to_string(s_id), std::to_string(static_cast<int>(JudgeStatus::UPDATED))};

		auto redis_conn_handler = sync_fetch_redis_conn();
		kerbal::redis_v2::connection & conn = *redis_conn_handler;
		auto res = conn.argv_execute(args.begin(), args.end());

//		if () {
//
//		}
//
//		if (res->)
//		LOG_WARNING(jobType, s_id, log_fp, "Source code doesn't exist!");
		// 可能 job 编译通过没有 compile_info, 所以不管有没有都 del 一次, 不过不用判断是否删除成功

	} catch (const std::exception & e) {

	}

} catch (...) {
	UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Clear this jobs info in redis failed!");
}


void ConcreteUpdateJob::core_update_failed_table() noexcept try
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



