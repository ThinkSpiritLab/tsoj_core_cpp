/*
 * ExerciseRejudgeJobBase.cpp
 *
 *  Created on: 2018年11月20日
 *      Author: peter
 */

#include <ExerciseRejudgeJobBase.hpp>

#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;

ExerciseRejudgeJobBase::ExerciseRejudgeJobBase(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn):
			RejudgeJobBase(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ExerciseRejudgeJobBase::move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	mysqlpp::Query insert = mysql_conn.query(
			"insert into rejudge_solution "
			"(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage) "
			"(select 0, solution.s_id, s_result, s_time, s_mem, s_similarity_percentage "
			"from solution "
			"where solution.s_id = %0)"
	);
	insert.parse();

	mysqlpp::SimpleResult res = insert.execute(s_id);

	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Insert original result into rejudge_solution failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 20_ms);
}

void ExerciseRejudgeJobBase::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

    mysqlpp::Query update = mysql_conn.query(
            "update solution "
            "set s_result = %0, s_time = %1, s_mem = %2, s_similarity_percentage = %3 "
            "where s_id = %4"
    );
    update.parse();
	mysqlpp::SimpleResult res = update.execute(ojv4::s_result_in_db_type(result.judge_result), result.cpu_time.count(),
												result.memory.count(), similarity_percentage, this->s_id);


	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 70_ms);
}

void ExerciseRejudgeJobBase::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	if (this->result.judge_result == ojv4::s_result_enum::COMPILE_ERROR) {

		kerbal::redis_v2::reply ce_info_reply;
		try {
			ce_info_reply = this->get_compile_info();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "Get compile error info failed!", e);
			throw;
		}

		mysqlpp::Query update = mysql_conn.query(
				"insert into compile_info(s_id, compile_error_info) "
				"values("
		);
		update << s_id << ','
				<< mysqlpp::quote << ce_info_reply->str << ')'
				<< "on duplicate key update compile_error_info = values(compile_error_info)";

		mysqlpp::SimpleResult res = update.execute();

		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile info failed!", e);
			throw e;
		}
	} else {
		mysqlpp::Query del = mysql_conn.query(
				"delete from compile_info "
				"where s_id = "
		);
		del << s_id;

		mysqlpp::SimpleResult res = del.execute();

		if (!res) {
			LOG_WARNING(jobType, s_id, log_fp, "Delete compile info failed!");
			// 删除失败可以不做 failed 处理
		}
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}
