/*
 * ExerciseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJob.hpp"
#include "logger.hpp"

namespace
{
	using namespace kerbal::redis;
	using namespace mysqlpp;
}

extern std::ostream log_fp;

ExerciseUpdateJob::ExerciseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
}

void ExerciseUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::update_solution");
	std::string solution_table = "solution";
	mysqlpp::Query templ = mysqlConn->query(
			"insert into %0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
	);
	templ.parse();
	mysqlpp::SimpleResult res = templ.execute(solution_table, sid, uid, pid, (int) lang, (int) result.judge_result,
												result.cpu_time.count(), result.memory.count(), postTime,
												result.similarity_percentage);
	if (!res) {
		throw MysqlEmptyResException(templ.errnum(), templ.error());
	}
}

user_problem_status ExerciseUpdateJob::get_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::get_user_problem_status");
	return this->supper_t::get_user_problem_status();
}

void ExerciseUpdateJob::update_user_and_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::update_user_and_problem");
	this->supper_t::update_user_and_problem();
}

void ExerciseUpdateJob::update_user_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::update_user_problem");
	this->supper_t::update_user_problem();
}
