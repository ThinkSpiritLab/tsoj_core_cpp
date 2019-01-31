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


ContestUpdateJob::ContestUpdateJob(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobBase(jobType, s_id, redis_conn), ct_id(ojv4::ct_id_type(jobType))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ContestUpdateJob::update_solution(mysqlpp::Connection & mysql_conn)
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
//	mysqlpp::SimpleResult res = insert.execute(ct_id, s_id, u_id, p_id, (int) lang, ojv4::s_result_in_db_type(result.judge_result),
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
			<< ojv4::s_result_in_db_type(result.judge_result) << ','
			<< result.cpu_time.count() << ','
			<< result.memory.count() << ','
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

void ContestUpdateJob::update_source_code(mysqlpp::Connection & mysql_conn)
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

void ContestUpdateJob::update_compile_info(mysqlpp::Connection & mysql_conn)
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

void ContestUpdateJob::update_user(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	// 竞赛更新 user 表的语义转变为更新榜单
	try {
		ContestManagement::update_scoreboard(mysql_conn, *sync_fetch_redis_conn(), ct_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update scoreboard failed!", e, ", ct_id: ", ct_id);
		return;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void ContestUpdateJob::update_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	try {
		ContestManagement::update_problem_s_submit_and_accept_num(mysql_conn, this->ct_id, this->p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem submit and accept number in contest failed!", e, ", ct_id: ", ct_id);
		throw;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void ContestUpdateJob::update_user_problem(mysqlpp::Connection & mysql_conn)
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


void ContestUpdateJob::update_user_problem_status(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	try {
		ContestManagement::update_user_problem_status(mysql_conn, this->ct_id, this->u_id, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem status failed!", e);
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 20_ms);
}
