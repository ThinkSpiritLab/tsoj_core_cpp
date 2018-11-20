/*
 * RejudgeJobBase.cpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#include "RejudgeJobBase.hpp"
#include "logger.hpp"

extern std::ofstream log_fp;

RejudgeJobBase::RejudgeJobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		UpdateJobBase(jobType, s_id, redisConn, std::move(mysqlConn)), rejudge_time(mysqlpp::DateTime::now())
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}


void RejudgeJobBase::handle()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	std::exception_ptr move_solution_exception = nullptr;
	ojv4::s_result_enum orig_result = ojv4::s_result_enum::ACCEPTED;
	try {
		orig_result = this->move_orig_solution_to_rejudge_solution();
	} catch (const std::exception & e) {
		move_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, s_id, log_fp, "Move original solution failed!", e);
		//DO NOT THROW
	}

	std::exception_ptr update_solution_exception = nullptr;
	std::exception_ptr update_user_exception = nullptr;
	std::exception_ptr update_problem_exception = nullptr;
	std::exception_ptr update_user_problem_exception = nullptr;
	std::exception_ptr update_user_problem_status_exception = nullptr;
	std::exception_ptr update_compile_error_info_exception = nullptr;
	if (move_solution_exception == nullptr) {

		try {
			this->update_solution();
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
			//DO NOT THROW
		}

		try {
			this->update_user();
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user failed!", e);
			//DO NOT THROW
		}

		try {
			this->update_problem();
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem failed!", e);
			//DO NOT THROW
		}

		try {
			this->update_user_problem();
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
			//DO NOT THROW
		}

		try {
			this->update_user_problem_status();
		} catch (const std::exception & e) {
			move_solution_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem status failed!", e);
			//DO NOT THROW
		}

		try {
			this->rejudge_compile_info(orig_result);
		} catch (const std::exception & e) {
			update_compile_error_info_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile error info failed!", e);
			//DO NOT THROW
		}

		try {
			this->send_rejudge_notification();
		} catch (const std::exception & e){
			EXCEPT_WARNING(jobType, s_id, log_fp, "Send rejudge notification failed!", e);
			//DO NOT THROW
		}

	}
}



