/*
 * CourseRejudgeJob.cpp
 *
 *  Created on: 2018年11月9日
 *	  Author: peter
 */

#include "CourseRejudgeJob.hpp"
#include "ExerciseManagement.hpp"
#include "CourseManagement.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;

CourseRejudgeJob::CourseRejudgeJob(int jobType, ojv4::s_id_type s_id, ojv4::c_id_type c_id, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		ExerciseRejudgeJobBase(jobType, s_id, redisConn, std::move(mysqlConn)), c_id(c_id)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}


void CourseRejudgeJob::update_user()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		CourseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->c_id, this->u_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's submit and accept num failed!", e);
		throw;
	}

	// 更新练习视角用户的提交数和通过数
	// 使课程中某个 user 的提交数与通过数同步到 user 表中
	try {
		ExerciseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->u_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's submit and accept number in exercise view failed!", e);
		throw;
	}
}

void CourseRejudgeJob::update_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		CourseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->c_id, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-problem's submit and accept num failed!", e);
		throw;
	}

	// 更新练习视角题目的提交数和通过数
	// 使课程中某个 problem 的提交数与通过数同步到 problem 表中
	try {
		ExerciseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-problem's submit and accept number in exercise view failed!", e);
		throw;
	}
}

void CourseRejudgeJob::update_user_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		CourseManagement::update_user_problem(*this->mysqlConn, this->c_id, this->u_id, this->p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user-problem failed!", e);
		throw;
	}

	// 更新练习视角的用户通过情况表
	try {
		ExerciseManagement::update_user_problem(*this->mysqlConn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user-problem in exercise view failed!", e);
		throw;
	}
}

void CourseRejudgeJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		CourseManagement::update_user_problem_status(*this->mysqlConn, this->c_id, this->u_id, this->p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's problem status failed!", e);
		throw;
	}

	// 更新练习视角的用户通过情况表
	try {
		ExerciseManagement::update_user_problem_status(*this->mysqlConn, this->u_id, this->p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update course-user's problem status in exercise view failed!", e);
		throw;
	}
}


void CourseRejudgeJob::send_rejudge_notification()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		mysqlpp::Query query = mysqlConn->query(
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

		mysqlpp::Query insert = mysqlConn->query(
				"insert into message (u_id, u_receiver, m_content, m_status) "
				"values (1, %0, %1q, %2)"
		);
		insert.parse();
		static boost::format message_templ("您于 %s 在课程《%s》中提交的问题 %d 的代码已经被重新评测，新的结果为 %s，请查询。");
		mysqlpp::SimpleResult res = insert.execute(u_id, (message_templ % s_posttime % c_name % p_id % getJudgeResultName(result.judge_result)).str(), 0b10100);
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
