/*
 * ExerciseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJob.hpp"
#include "logger.hpp"
#include "ExerciseManagement.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>


extern std::ofstream log_fp;


ExerciseJobInterface::ExerciseJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ExerciseJobInterface::update_user(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	ExerciseManagement::update_user_s_sa_num(mysql_conn, this->u_id);

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 30_ms);
}

void ExerciseJobInterface::update_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD
	ExerciseManagement::update_problem_s_sa_num(mysql_conn, this->p_id);
	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 30_ms);
}

void ExerciseJobInterface::update_user_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD
	ExerciseManagement::update_user_problem(mysql_conn, u_id, p_id);
	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

BasicExerciseJobView::BasicExerciseJobView(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		BasicJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void BasicExerciseJobView::update_source_code(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	kerbal::redis_v2::reply reply = this->get_source_code();
	const char * source_code = reply->str;

	if (source_code == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty source code!");
		return;
	}

//	mysqlpp::Query insert = mysqlConn->query(
//			"insert into source (s_id, source_code)"
//			"values (%0, %1q)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(s_id, source_code);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into source (s_id, source_code) "
			"values("
	);
	insert << s_id << ','
			<< mysqlpp::quote << source_code << ')';
	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update source code failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

void BasicExerciseJobView::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	kerbal::redis_v2::reply reply = this->get_compile_info();
	const char * compile_info = reply->str;

	if (compile_info == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty compile info!");
		return;
	}

//	mysqlpp::Query insert = mysqlConn->query(
//			"insert into compile_info (s_id, compile_error_info) "
//			"values (%0, %1q)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(s_id, compile_info);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into compile_info (s_id, compile_error_info) "
			"values("
	);
	insert << s_id << ','
			<< mysqlpp::quote << compile_info << ')';
	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile info failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

BasicExerciseJob::BasicExerciseJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		BasicExerciseJobView(jobType, s_id, redis_conn),
		ExerciseJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void BasicExerciseJob::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

//	mysqlpp::Query insert = mysqlConn->query(
//			"insert into solution "
//			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage) "
//			"values(%0, %1, %2, %3, %4, %5, %6, %7q, %8)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(s_id, u_id, p_id, (int) lang, (int) result.judge_result,
//												(int)result.cpu_time.count(), (int)result.memory.count(), s_posttime,
//												similarity_percentage);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into solution "
			"(s_id, u_id, p_id, s_lang, "
			" s_result, s_time, s_mem, "
			" s_posttime, s_similarity_percentage) "
			"values("
	);
	insert << s_id << ',' << u_id << ',' << p_id << ',' << int(lang) << ','
			<< int(this->judge_result) << ',' << int(this->cpu_time.count()) << ',' << int(this->memory.count()) << ','
			<< mysqlpp::quote << s_posttime << ',' << similarity_percentage << ')';

	mysqlpp::SimpleResult res = insert.execute();
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 45_ms);
}

RejudgeExerciseJobView::RejudgeExerciseJobView(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		RejudgeJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void RejudgeExerciseJobView::move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	mysqlpp::Query insert = mysql_conn.query(
			"insert into rejudge_solution "
			"(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage) "
			"(		select 0, solution.s_id, s_result, s_time, s_mem, s_similarity_percentage "
			"		from solution "
			"		where solution.s_id = %0)"
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

void RejudgeExerciseJobView::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	mysqlpp::Query update = mysql_conn.query(
				"update solution "
				"set s_result = %0, s_time = %1, s_mem = %2, s_similarity_percentage = %3 "
				"where s_id = %4"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(oj::s_result_in_db_type(this->judge_result), this->cpu_time.count(),
			this->memory.count(), similarity_percentage, this->s_id);


	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 70_ms);
}

void RejudgeExerciseJobView::update_compile_info(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

	if (this->judge_result == oj::s_result_enum::COMPILE_ERROR) {

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
			// 删除失败可以不做 failed 处理. 另, 要是原表中没有此条 solution 的 ce 信息, 也不会删除失败
		}
	}

	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms);
}

RejudgeExerciseJob::RejudgeExerciseJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		RejudgeExerciseJobView(jobType, s_id, redis_conn),
		ExerciseJobInterface(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void RejudgeExerciseJob::send_rejudge_notification(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into message (u_id, u_receiver, m_content, m_status) "
			"values (1, %0, %1q, %2)"
	);
	insert.parse();
	boost::format message_templ("您于 %s 提交的问题 %d 的代码已经被重新评测，新的结果为 %s，请查询。");
	mysqlpp::SimpleResult res = insert.execute(u_id, (message_templ % s_posttime % p_id % getJudgeResultName(this->judge_result)).str(), 0b10100);
	//0b10100 means bold font and closed.
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Insert into message failed!", e);
		throw e;
	}
}


