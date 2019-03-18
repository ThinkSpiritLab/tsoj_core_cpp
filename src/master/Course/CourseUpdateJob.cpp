/*
 * CourseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "CourseUpdateJob.hpp"
#include "logger.hpp"
#include "CourseManagement.hpp"
#include "ExerciseManagement.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;


CourseJobInterface::CourseJobInterface(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn) :
				ExerciseJobInterface(jobType, s_id, redis_conn),
				c_id(c_id)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void CourseJobInterface::update_user(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		PROFILE_HEAD
		CourseManagement::update_user_s_sa_num(mysql_conn, this->c_id, this->u_id);
		PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 30_ms, "CourseManagement::update_user_s_sa_num");
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's submit and accept number failed!", e);
		throw;
	}

	// 更新练习视角用户的提交数和通过数
	// 使课程中某个 user 的提交数与通过数同步到 user 表中
	try {
		this->ExerciseJobInterface::update_user(mysql_conn);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's submit and accept number in exercise view failed!", e);
		throw;
	}
}

void CourseJobInterface::update_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		PROFILE_HEAD
		CourseManagement::update_problem_s_sa_num(mysql_conn, this->c_id, this->p_id);
		PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 30_ms, "CourseManagement::update_problem_s_sa_num");
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-problem's submit and accept number failed!", e);
		throw;
	}

	// 更新练习视角题目的提交数和通过数
	// 使课程中某个 problem 的提交数与通过数同步到 problem 表中
	try {
		this->ExerciseJobInterface::update_problem(mysql_conn);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-problem's submit and accept number in exercise view failed!", e);
		throw;
	}
}


void CourseJobInterface::update_user_problem(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		PROFILE_HEAD
		CourseManagement::update_user_problem(mysql_conn, this->c_id, this->u_id, this->p_id);
		PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms, "CourseManagement::update_user_problem");
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user-problem failed!", e);
		throw;
	}

	// 更新练习视角的用户通过情况表
	try {
		this->ExerciseJobInterface::update_user_problem(mysql_conn);
		PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 50_ms, "ExerciseManagement::update_user_problem");
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user-problem in exercise view failed!", e);
		throw;
	}
}

BasicCourseJob::BasicCourseJob(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		BasicExerciseJobView(jobType, s_id, redis_conn),
		CourseJobInterface(jobType, s_id, c_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void BasicCourseJob::update_solution(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
	PROFILE_HEAD

//	mysqlpp::Query insert = mysqlConn->query(
//			"insert into solution "
//			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, c_id, s_similarity_percentage) "
//			"values(%0, %1, %2, %3, %4, %5, %6, %7q, %8, %9)"
//	);
//	insert.parse();
//	mysqlpp::SimpleResult res = insert.execute(s_id, u_id, p_id, (int) lang, (int) result.judge_result,
//											(int)result.cpu_time.count(), (int)result.memory.count(), s_posttime, c_id,
//												similarity_percentage);

	mysqlpp::Query insert = mysql_conn.query(
			"insert into solution "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, c_id, s_similarity_percentage) "
			"values("
	);
	insert << s_id << ',' << u_id << ',' << p_id << ',' << int(lang) << ','
			<< int(this->judge_result) << ',' << int(this->cpu_time.count()) << ',' << int(this->memory.count()) << ','
			<< mysqlpp::quote << s_posttime << ','
			<< c_id << ','
			<< int(similarity_percentage) << ')';
	mysqlpp::SimpleResult res = insert.execute();

	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}
	PROFILE_WARNING_TAIL(jobType, s_id, log_fp, 45_ms);
}

RejudgeCourseJob::RejudgeCourseJob(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn) :
		UpdateJobInterface(jobType, s_id, redis_conn),
		RejudgeExerciseJobView(jobType, s_id, redis_conn),
		CourseJobInterface(jobType, s_id, c_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void RejudgeCourseJob::send_rejudge_notification(mysqlpp::Connection & mysql_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		mysqlpp::Query query = mysql_conn.query(
				"select c_name from course where c_id = %0"
		);
		query.parse();
		mysqlpp::StoreQueryResult r = query.store(c_id);
		if (r.empty()) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Select course name failed!", e);
			throw e;
		}

		std::string c_name(r[0]["c_name"]);

		mysqlpp::Query insert = mysql_conn.query(
				"insert into message (u_id, u_receiver, m_content, m_status) "
				"values (1, %0, %1q, %2)"
		);
		insert.parse();
		boost::format message_templ("您于 %s 在课程《%s》中提交的问题 %d 的代码已经被重新评测，新的结果为 %s，请查询。");
		mysqlpp::SimpleResult res = insert.execute(u_id, (message_templ % s_posttime % c_name % p_id % getJudgeResultName(this->judge_result)).str(), 0b10100);
		//0b10100 means bold font and closed.
		if (!res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Insert into message failed!", e);
			throw e;
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Send rejudge notification failed!", e);
		throw;
	}
}

