/*
 * ExerciseRejudgeJob.cpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#include "ExerciseRejudgeJob.hpp"
#include "ExerciseManagement.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;

ExerciseRejudgeJob::ExerciseRejudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		ExerciseRejudgeJobBase(jobType, s_id, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ExerciseRejudgeJob::update_user()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->u_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseRejudgeJob::update_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseRejudgeJob::update_user_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_problem(*this->mysqlConn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseRejudgeJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_problem_status(*this->mysqlConn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}


void ExerciseRejudgeJob::send_rejudge_notification()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		mysqlpp::Query insert = mysqlConn->query(
				 "insert into message (u_id, u_receiver, m_content, m_status) "
				 "values (1, %0, %1q, %2)"
		);
		insert.parse();
		static boost::format message_templ("您于 %s 提交的问题 %d 的代码已经被重新评测，新的结果为 %s，请查询。");
		mysqlpp::SimpleResult res = insert.execute(u_id, (message_templ % s_posttime % p_id % getJudgeResultName(result.judge_result)).str(), 0b10100);
		//0b10100 means bold font and closed.
		if(!res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(jobType, s_id, log_fp, "Insert into message failed!", e);
			throw e;
		}
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Send rejudge notification failed!", e);
		throw;
	}
}



