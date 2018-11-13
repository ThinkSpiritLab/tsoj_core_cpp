/*
 * ContestUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ContestUpdateJob.hpp"
#include "boost_format_suffix.hpp"
#include "logger.hpp"
#include "ContestManagement.hpp"

#include <kerbal/redis/redis_data_struct/list.hpp>

extern std::ofstream log_fp;


ContestUpdateJob::ContestUpdateJob(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn,
		std::unique_ptr <mysqlpp::Connection> && mysqlConn) : supper_t(jobType, s_id, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, s_id, log_fp, "ContestUpdateJob::ContestUpdateJob");
}

void ContestUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_solution");

	// 每个 solution 之前均不存在，使用 insert
	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_solution%0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, u_id, pid, (int) lang, (int) result.judge_result,
												(int)result.cpu_time.count(), (int)result.memory.count(), s_posttime,
												similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_source_code(const char * source_code)
{
	if (source_code == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty source code!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_source%0 (s_id, source_code)"
			"values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, source_code);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update source code failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_compile_info(const char * compile_info)
{
	if (compile_info == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty compile info!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_compile_info%0 (s_id, compile_error_info) values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, compile_info);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update compile info failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_user()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user");

	// 竞赛更新 user 表的语义转变为更新榜单
	try {
		kerbal::redis::List<ojv4::ct_id_type> update_contest_scoreboard_queue(this->redisConn, "update_contest_scoreboard_queue");
		update_contest_scoreboard_queue.push_back(jobType);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Insert update scoreboard job failed!", e);
		return;
	}

}

void ContestUpdateJob::update_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_problem");

	try {
		ContestManagement::update_problem_submit_and_accept_num(*this->mysqlConn, this->jobType, this->pid);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update problem submit and accept number in contest failed!", e, ", ct_id: ", this->jobType);
		throw;
	}
}

void ContestUpdateJob::update_user_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user_problem");

	try {
		ContestManagement::update_user_problem(*this->mysqlConn, this->jobType, this->u_id, this->pid);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user problem failed!", e);
	}
}


void ContestUpdateJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user_problem_status");

	try {
		ContestManagement::update_user_problem_status(*this->mysqlConn, this->jobType, this->u_id, this->pid);
	} catch(const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user problem status failed!", e);
	}
}
