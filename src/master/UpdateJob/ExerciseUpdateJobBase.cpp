/*
 * ExerciseUpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJobBase.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>


extern std::ofstream log_fp;


ExerciseUpdateJobBase::ExerciseUpdateJobBase(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
				UpdateJobBase(jobType, s_id, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void ExerciseUpdateJobBase::update_source_code(const char * source_code)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	if (source_code == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty source code!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into source (s_id, source_code)"
			"values (%0, %1q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(s_id, source_code);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update source code failed!", e);
		throw e;
	}
}

void ExerciseUpdateJobBase::update_compile_info(const char * compile_info)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	if (compile_info == nullptr) {
		LOG_WARNING(jobType, s_id, log_fp, "empty compile info!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into compile_info (s_id, compile_error_info) "
			"values (%0, %1q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(s_id, compile_info);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile info failed!", e);
		throw e;
	}
}

