/*
 * ContestRejudgeJob.cpp
 *
 *  Created on: 2018年11月20日
 *      Author: peter
 */

#include "ContestRejudgeJob.hpp"
#include "ContestManagement.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;

ContestRejudgeJob::ContestRejudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn) :
		RejudgeJobBase(jobType, s_id, redisConn), ct_id(ojv4::ct_id_type(jobType))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ContestRejudgeJob::move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	ojv4::s_result_enum orig_result;
	ojv4::s_time_in_milliseconds orig_time;
	ojv4::s_mem_in_byte orig_mem;
	ojv4::s_similarity_type orig_similarity;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select s_result, s_time, s_mem, s_similarity_percentage "
				"from contest_solution%0 where s_id = %1");
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(ct_id, s_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Query original result failed!", e);
			throw e;
		}

		if (res.empty()) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Empty original result!", e);
			throw e;
		}

		const auto & row = res[0];
		orig_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
		orig_time = ojv4::s_time_in_milliseconds(row["s_time"]);
		orig_mem = ojv4::s_mem_in_byte(row["s_mem"]);
		orig_similarity = row["s_similarity_percentage"];
	}

	mysqlpp::Query insert = mysql_conn.query(
			"insert into rejudge_solution "
			"(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage, rejudge_time) "
			"values(%0, %1, %2, %3, %4, %5, %6q)"
	);
	insert.parse();

	mysqlpp::SimpleResult res = insert.execute(ct_id, s_id, ojv4::s_result_in_db_type(orig_result), orig_time.count(),
												orig_mem.count(), orig_similarity, rejudge_time);

	if(!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Insert original result into rejudge_solution failed!", e);
		throw e;
	}
}

void ContestRejudgeJob::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

    mysqlpp::Query update = mysql_conn.query(
            "update contest_solution%0 "
            "set s_result = %1, s_time = %2, s_mem = %3, s_similarity_percentage = %4 "
            "where s_id = %5"
    );
    update.parse();
	mysqlpp::SimpleResult res = update.execute(ct_id, ojv4::s_result_in_db_type(result.judge_result), result.cpu_time.count(),
												result.memory.count(), similarity_percentage, this->s_id);


	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}
}

void ContestRejudgeJob::update_user(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ContestRejudgeJob::update_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ContestManagement::update_problem_s_submit_and_accept_num(mysql_conn, ct_id, p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ContestRejudgeJob::update_user_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ContestManagement::update_user_problem(mysql_conn, ct_id, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ContestRejudgeJob::update_user_problem_status(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ContestManagement::update_user_problem_status(mysql_conn, ct_id, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ContestRejudgeJob::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	if (this->result.judge_result == ojv4::s_result_enum::COMPILE_ERROR) {

		kerbal::redis::RedisReply ce_info;
		try {
			ce_info = this->get_compile_info();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "Get compile error info failed!", e);
			throw;
		}

		mysqlpp::Query update = mysql_conn.query(
				"insert into contest_compile_info%0(s_id, compile_error_info) "
				"values(%1, %2q) "
				"on duplicate key "
				"update compile_error_info = %2q"
		);
		update.parse();

		mysqlpp::SimpleResult res = update.execute(ct_id, s_id, ce_info->str);

		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile info failed!", e);
			throw e;
		}
	} else {
		mysqlpp::Query del = mysql_conn.query(
				"delete from contest_compile_info%0 "
				"where s_id = %1"
		);
		del.parse();

		mysqlpp::SimpleResult res = del.execute(ct_id, s_id);

		if (!res) {
			LOG_WARNING(jobType, s_id, log_fp, "Delete compile info failed! Info: ", del.error());
			// 删除失败可以不做 failed 处理
		}
	}
}
