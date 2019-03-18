/*
 * ContestUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ContestUpdateJob.hpp"
#include "boost_format_suffix.hpp"
#include "redis_conn_factory.hpp"
#include "logger.hpp"
#include "ContestManagement.hpp"


#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;


ContestUpdateJobInterface::ContestUpdateJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn), ct_id(oj::ct_id_type(jobType))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}


void ContestUpdateJobInterface::update_user(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	// 竞赛更新 user 表的语义转变为更新榜单
	try {
		auto redis_conn_handler = sync_fetch_redis_conn();
		ContestManagement::update_scoreboard(mysql_conn, *redis_conn_handler, ct_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update scoreboard failed!", e, ", ct_id: ", ct_id);
		return;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void ContestUpdateJobInterface::update_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	try {
		ContestManagement::update_problem_s_sa_num(mysql_conn, this->ct_id, this->p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem submit and accept number in contest failed!", e, ", ct_id: ", ct_id);
		throw;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void ContestUpdateJobInterface::update_user_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	try {
		ContestManagement::update_user_problem(mysql_conn, this->ct_id, this->u_id, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

BasicContestJob::BasicContestJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		BasicJobInterface(jobType, s_id, redis_conn),
		ContestUpdateJobInterface(jobType, s_id, redis_conn)
{
}

void BasicContestJob::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	// 每个 solution 之前均不存在，使用 insert
//	mysqlpp::Query insert = mysql_conn.query(
//			"insert into contest_solution%0 "
//			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
//			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(ct_id, s_id, u_id, p_id, (int) lang, oj::s_result_in_db_type(result.judge_result),
//												result.cpu_time.count(), result.memory.count(), s_posttime,
//												similarity_percentage);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into contest_solution"
	);

	insert << ct_id
			<< "(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage) values("
			<< s_id << ','
			<< u_id << ','
			<< p_id << ','
			<< int(lang) << ','
			<< oj::s_result_in_db_type(this->judge_result) << ','
			<< this->cpu_time.count() << ','
			<< this->memory.count() << ','
			<< mysqlpp::quote << s_posttime << ','
			<< similarity_percentage << ')';

	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 45_ms);
}

void BasicContestJob::update_source_code(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	kerbal::redis_v2::reply reply = this->get_source_code();
	const char * source_code = reply->str;

	if (source_code == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty source code!");
		return;
	}

//	mysqlpp::Query insert = mysql_conn.query(
//			"insert into contest_source%0 (s_id, source_code)"
//			"values (%1, %2q)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(ct_id, s_id, source_code);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into contest_source"
	);
	insert << ct_id
			<<"(s_id, source_code) values("
			<< s_id << ','
			<< mysqlpp::quote << source_code << ')';

	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update source code failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void BasicContestJob::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	kerbal::redis_v2::reply reply = this->get_compile_info();
	const char * compile_info = reply->str;

	if (compile_info == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty compile info!");
		return;
	}

//	mysqlpp::Query insert = mysql_conn.query(
//			"insert into contest_compile_info%0 (s_id, compile_error_info) "
//			"values (%1, %2q)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(ct_id, s_id, compile_info);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into contest_compile_info"
	);
	insert << ct_id
			<< "(s_id, compile_error_info) values("
			<< s_id << ','
			<< mysqlpp::quote << compile_info << ')';

	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile info failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}


RejudgeContestJob::RejudgeContestJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		RejudgeJobInterface(jobType, s_id, redis_conn),
		ContestUpdateJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void RejudgeContestJob::move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into rejudge_solution "
			"(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage) "
			"(		select %0, solution.s_id as ct_s_id, s_result, s_time, s_mem, s_similarity_percentage "
			"		from contest_solution%1 "
			"		where ct_s_id = %2)"
	);
	insert.parse();

	mysqlpp::SimpleResult res = insert.execute(ct_id, ct_id, s_id);

	if(!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Insert original result into rejudge_solution failed!", e);
		throw e;
	}
}

void RejudgeContestJob::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	mysqlpp::Query update = mysql_conn.query(
				"update contest_solution%0 "
				"set s_result = %1, s_time = %2, s_mem = %3, s_similarity_percentage = %4 "
				"where s_id = %5"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(ct_id, oj::s_result_in_db_type(this->judge_result), this->cpu_time.count(),
			this->memory.count(), similarity_percentage, this->s_id);


	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}
}


void RejudgeContestJob::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	if (this->judge_result == oj::s_result_enum::COMPILE_ERROR) {

		kerbal::redis_v2::reply ce_info_reply;
		try {
			ce_info_reply = this->get_compile_info();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "Get compile error info failed!", e);
			throw;
		}

		mysqlpp::Query update = mysql_conn.query(
				"insert into contest_compile_info%0(s_id, compile_error_info) "
				"values(%1, %2q) "
				"on duplicate key "
				"update compile_error_info = values(compile_error_info)"
		);
		update.parse();

		mysqlpp::SimpleResult res = update.execute(ct_id, s_id, ce_info_reply->str);

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

