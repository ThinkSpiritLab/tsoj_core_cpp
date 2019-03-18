/*
 * RejudgeJobInterface.cpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#include "RejudgeJobInterface.hpp"
#include "logger.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/transaction.h>

#include "mysql_conn_factory.hpp"

extern std::ofstream log_fp;


RejudgeJobInterface::RejudgeJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		ConcreteUpdateJob(jobType, s_id, redis_conn),
		rejudge_time(mysqlpp::DateTime::now())
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void RejudgeJobInterface::handle()
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);

	std::exception_ptr move_solution_exception = nullptr;
	std::exception_ptr update_solution_exception = nullptr;
	std::exception_ptr update_compile_info_exception = nullptr;
	std::exception_ptr update_user_exception = nullptr;
	std::exception_ptr update_problem_exception = nullptr;
	std::exception_ptr update_user_problem_exception = nullptr;
	std::exception_ptr commit_exception = nullptr;
	std::reference_wrapper<const std::exception_ptr> exception_ptrs[] = {
		move_solution_exception,
		update_solution_exception,
		update_compile_info_exception,
		update_user_exception,
		update_problem_exception,
		update_user_problem_exception,
		commit_exception
	};


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

			if (this->judge_result == UnitedJudgeResult::COMPILE_ERROR) {
				try {
					this->update_compile_info(mysql_conn);
				} catch (const std::exception & e) {
					update_compile_info_exception = std::current_exception();
					EXCEPT_FATAL(jobType, s_id, log_fp, "Update compile error info failed!", e);
					//DO NOT THROW
				}
			}

//			try {
//				this->update_user(mysql_conn);
//			} catch (const std::exception & e) {
//				update_user_exception = std::current_exception();
//				EXCEPT_FATAL(jobType, s_id, log_fp, "Update user failed!", e);
//				//DO NOT THROW
//			}
//
//			try {
//				this->update_problem(mysql_conn);
//			} catch (const std::exception & e) {
//				update_problem_exception = std::current_exception();
//				EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem failed!", e);
//				//DO NOT THROW
//			}

			try {
				this->update_user_problem(mysql_conn);
			} catch (const std::exception & e) {
				update_user_problem_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
				//DO NOT THROW
			}

			try {
				this->send_rejudge_notification(mysql_conn);
			} catch (const std::exception & e){
				EXCEPT_WARNING(jobType, s_id, log_fp, "Send rejudge notification failed!", e);
				//DO NOT THROW
			}

			try {
				trans.commit();
			} catch (const std::exception & e) {
				commit_exception = std::current_exception();
				EXCEPT_FATAL(jobType, s_id, log_fp, "MySQL commit failed!", e);
				//DO NOT THROW
			}
		}
	}

	if (std::all_of(std::begin(exception_ptrs), std::end(exception_ptrs),
		[](const std::exception_ptr & ep) noexcept { // is_nullptr
			return ep == nullptr;
	}) == true) {
		this->clear_redis_info();
		this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);
	} else {
		LOG_WARNING(jobType, s_id, log_fp, "Error occurred while update this job!");
	}

}

void RejudgeJobInterface::clear_redis_info() noexcept try
{
	try {
		std::initializer_list<std::string> args = {"EVAL", R"===(
			local JUDGE_STATUS__UPDATED = ARGV[3]
			local job_type = ARGV[1]
			local s_id = ARGV[2]
			local delete_result = {0, 0, 0}
			local judge_status = string.format('judge_status:%d:%d', job_type, s_id)
			if (redis.call('hget', judge_status, 'status') == JUDGE_STATUS__UPDATED) then
				delete_result[1] = redis.call('del', judge_status)															# delete judge_status:%d:%d
				delete_result[2] = redis.call('del', string.format('similarity_details:%d:%d', job_type, s_id))	# delete similarity_details:%d:%d
				delete_result[3] = redis.call('del', string.format('job_info:%d:%d', job_type, s_id))				# delete job_info:%d:%d
			end
			return delete_result
		)===", "0", std::to_string(jobType), std::to_string(s_id), std::to_string(static_cast<int>(JudgeStatus::UPDATED))};

		auto redis_conn_handler = sync_fetch_redis_conn();
		kerbal::redis_v2::connection & conn = *redis_conn_handler;
		auto res = conn.argv_execute(args.begin(), args.end());

//		if () {
//
//		}
//
//		if (res->)
//		LOG_WARNING(jobType, s_id, log_fp, "`judge_status` doesn't delete actually!");
//		LOG_WARNING(jobType, s_id, log_fp, "`similarity_details` doesn't delete actually!");
//		LOG_WARNING(jobType, s_id, log_fp, "`job_info` doesn't delete actually!");

	} catch (const std::exception & e) {

	}

} catch (...) {
	UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Clear this jobs info in redis failed!");
}



