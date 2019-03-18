/*
 * BasicUpdateJobInterface.cpp
 *
 *  Created on: 2019年3月11日
 *      Author: peter
 */

#include "BasicUpdateJobInterface.hpp"

#include "redis_conn_factory.hpp"
#include "mysql_conn_factory.hpp"
#include "logger.hpp"

extern std::ofstream log_fp;

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/transaction.h>

BasicJobInterface::BasicJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		ConcreteUpdateJob(jobType, s_id, redis_conn)
{
	LOG_DEBUG(jobType, s_id, log_fp, BOOST_CURRENT_FUNCTION);
}

void BasicJobInterface::handle()
{
	std::exception_ptr update_solution_exception = nullptr;
	std::exception_ptr update_source_code_exception = nullptr;
	std::exception_ptr update_compile_info_exception = nullptr;
	std::exception_ptr update_user_exception = nullptr;
	std::exception_ptr update_problem_exception = nullptr;
	std::exception_ptr update_user_problem_exception = nullptr;
	std::exception_ptr commit_exception = nullptr;
	std::reference_wrapper<const std::exception_ptr> exception_ptrs[] = {
		update_solution_exception,
		update_source_code_exception,
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

	// Step 1: 在 solution 表中插入本 solution 记录
	try {
		this->update_solution(mysql_conn);
	} catch (const std::exception & e) {
		update_solution_exception = std::current_exception();
		EXCEPT_FATAL(jobType, s_id, log_fp, "Update solution failed!", e);
		//DO NOT THROW
	}

	if (update_solution_exception == nullptr) {

		// Step 2: 在 source 表或 contest_source%d 表中留存用户代码
		try {
			this->update_source_code(mysql_conn);
		} catch (const std::exception & e) {
			update_source_code_exception = std::current_exception();
			EXCEPT_WARNING(jobType, s_id, log_fp, "Update source code failed!", e);
			//DO NOT THROW
		}

		// Step 3: 在 compile_info 表或 contest_compile_info%d 表中留存编译错误信息
		if (this->judge_result == UnitedJudgeResult::COMPILE_ERROR) {
			try {
				this->update_compile_info(mysql_conn);
			} catch (const std::exception & e) {
				update_compile_info_exception = std::current_exception();
				EXCEPT_WARNING(jobType, s_id, log_fp, "Update compile info failed!", e);
				//DO NOT THROW
			}
		}

		// Step 4: 更新/插入 user_problem 表中该用户对应该题目的解题状态
		try {
			this->update_user_problem(mysql_conn);
		} catch (const std::exception & e) {
			update_user_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user problem failed!", e);
			//DO NOT THROW
		}

		// Step 5: 更新用户的提交数与通过数
		//注意!!! 更新用户提交数通过数的操作必须在更新用户题目完成状态成功之前
		try {
			this->update_user(mysql_conn);
		} catch (const std::exception & e) {
			update_user_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update user failed!", e);
			//DO NOT THROW
		}

		// Step 6: 更新题目的提交数与通过数
		//注意!!! 更新题目提交数通过数的操作必须在更新用户题目完成状态成功之前
		try {
			this->update_problem(mysql_conn);
		} catch (const std::exception & e) {
			update_problem_exception = std::current_exception();
			EXCEPT_FATAL(jobType, s_id, log_fp, "Update problem failed!", e);
			//DO NOT THROW
		}

		try {
			trans.commit();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, s_id, log_fp, "MySQL commit failed!", e);
			commit_exception = std::current_exception();
			//DO NOT THROW
		}

	}

	// Step 7: 若所有表都更新成功，则本次同步到 mysql 的操作成功，
	//         应将 redis 数据库中本 solution 的无用的信息删除。
	if (std::all_of(std::begin(exception_ptrs), std::end(exception_ptrs),
		[](const std::exception_ptr & ep) noexcept { // is_nullptr
			return ep == nullptr;
	}) == true) {
		this->clear_this_jobs_info_in_redis();
		this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);
	} else {
		LOG_WARNING(jobType, s_id, log_fp, "Error occurred while update this job!");
	}

	// Step 8: 清除 redis 中过于久远的 solution 数据, 这些 redis 缓存过于久远以致 Web 不大可能访问的到
	this->clear_previous_jobs_info_in_redis();

}

void BasicJobInterface::clear_previous_jobs_info_in_redis() noexcept try
{
	constexpr int REDIS_SOLUTION_MAX_NUM = 600;

	if (this->s_id.to_literal() <= REDIS_SOLUTION_MAX_NUM) {
		return;
	}
	int s_id = this->s_id.to_literal() - REDIS_SOLUTION_MAX_NUM;

	std::initializer_list<std::string> args = {"EVAL", R"===(
		local JUDGE_STATUS__UPDATED = ARGV[3]
		local job_type = ARGV[1]
		local s_id = ARGV[2]
		local delete_result = {0, 0, 0}
		local judge_status = string.format('judge_status:%d:%d', job_type, s_id)
		if (redis.call('hget', judge_status, 'status') == JUDGE_STATUS__UPDATED) then
			delete_result[1] = redis.call('del', judge_status) 															# delete judge_status:%d:%d-600
			delete_result[2] = redis.call('del', string.format('similarity_details:%d:%d', job_type, s_id))	# delete similarity_details:%d:%d-600
			delete_result[3] = redis.call('del', string.format('job_info:%d:%d', job_type, s_id))				# delete job_info:%d:%d-600
		end
		return delete_result
	)===", "0",
	std::to_string(jobType), std::to_string(s_id),
	std::to_string(static_cast<int>(JudgeStatus::UPDATED))};


	/*
	EXCEPT_FATAL(jobType, s_id, log_fp, "Get judge status failed!", e);
	LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete judge_status actually!");
	//LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete similarity_details actually!");
	LOG_WARNING(jobType, s_id, log_fp, "Doesn't delete job_info actually!");
	*/

} catch(...) {
	UNKNOWN_EXCEPT_FATAL(jobType, this->s_id, log_fp, "Clear previous jobs info in redis failed!");
	//DO NOT THROW BECAUSE THIS METHOD HAS BEEN DECLEARED AS NOEXCEPT
}

