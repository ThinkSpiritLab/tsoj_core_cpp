/*
 * RejudgeJobBase.cpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#include "RejudgeJobBase.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/transaction.h>

#include <mysql_conn_factory.hpp>

extern std::ofstream log_fp;

RejudgeJobBase::RejudgeJobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn) :
		UpdateJobBase(jobType, s_id, redisConn), rejudge_time(mysqlpp::DateTime::now())
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}


void RejudgeJobBase::handle()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	std::exception_ptr move_solution_exception = nullptr;
	std::exception_ptr update_solution_exception = nullptr;
	std::exception_ptr update_compile_error_info_exception = nullptr;
	std::exception_ptr update_user_exception = nullptr;
	std::exception_ptr update_problem_exception = nullptr;
	std::exception_ptr update_user_problem_exception = nullptr;
	std::exception_ptr update_user_problem_status_exception = nullptr;
	std::exception_ptr commit_exception = nullptr;

	// 本次更新任务的 mysql 连接
	auto mysql_conn_handle = sync_fetch_mysql_conn();
	mysqlpp::Connection & mysql_conn = *mysql_conn_handle;

	mysqlpp::Transaction trans(mysql_conn);
	try {
		this->move_orig_solution_to_rejudge_solution(mysql_conn);
	} catch (const std::exception & e) {
		move_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, s_id, log_fp, "Move original solution failed!", e);
		//DO NOT THROW
	}

	if (move_solution_exception == nullptr) {

		try {
			this->update_solution(mysql_conn);
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
			//DO NOT THROW
		}

		if (update_solution_exception == nullptr) {
			try {
				this->update_compile_info(mysql_conn);
			} catch (const std::exception & e) {
				update_compile_error_info_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile error info failed!", e);
				//DO NOT THROW
			}

			try {
				this->update_user(mysql_conn);
			} catch (const std::exception & e) {
				update_user_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "Update user failed!", e);
				//DO NOT THROW
			}

			try {
				this->update_problem(mysql_conn);
			} catch (const std::exception & e) {
				update_problem_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem failed!", e);
				//DO NOT THROW
			}

			try {
				this->update_user_problem(mysql_conn);
			} catch (const std::exception & e) {
				update_user_problem_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
				//DO NOT THROW
			}

//			try {
//				this->update_user_problem_status(mysql_conn);
//			} catch (const std::exception & e) {
//				update_user_problem_status_exception = std::current_exception();
//				EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem status failed!", e);
//				//DO NOT THROW
//			}

			try {
				this->send_rejudge_notification(mysql_conn);
			} catch (const std::exception & e){
				EXCEPT_WARNING(jobType, s_id, log_fp, "Send rejudge notification failed!", e);
				//DO NOT THROW
			}

		}

		try {
			trans.commit();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "MySQL commit failed!", e);
			commit_exception = std::current_exception();
			//DO NOT THROW
		} catch (...) {
			UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "MySQL commit failed!");
			commit_exception = std::current_exception();
			//DO NOT THROW
		}
	}
}



