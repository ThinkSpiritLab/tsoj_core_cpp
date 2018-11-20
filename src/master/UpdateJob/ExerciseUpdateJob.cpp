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


ExerciseUpdateJob::ExerciseUpdateJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		ExerciseUpdateJobBase(jobType, s_id, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ExerciseUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	mysqlpp::Query insert = mysqlConn->query(
			"insert into solution "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%0, %1, %2, %3, %4, %5, %6, %7q, %8)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(s_id, u_id, p_id, (int) lang, (int) result.judge_result,
												(int)result.cpu_time.count(), (int)result.memory.count(), s_posttime,
												similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		throw e;
	}
}

void ExerciseUpdateJob::update_user()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->u_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update user's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseUpdateJob::update_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->p_id);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseUpdateJob::update_user_problem()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_problem(*this->mysqlConn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}

void ExerciseUpdateJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	try {
		ExerciseManagement::update_user_problem_status(*this->mysqlConn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem's submit and accept num failed!", e);
		throw;
	}
}


