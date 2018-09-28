/*
 * ExerciseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJob.hpp"
#include "logger.hpp"

extern std::ostream log_fp;


ExerciseUpdateJob::ExerciseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
		supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::ExerciseUpdateJob");
}

void ExerciseUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::update_solution");
	mysqlpp::Query insert = mysqlConn->query(
			"insert into solution "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%0, %1, %2, %3, %4, %5, %6, %7q, %8)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(sid, uid, pid, (int) lang, (int) result.judge_result,
												(int)result.cpu_time.count(), (int)result.memory.count(), postTime,
												result.similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		throw e;
	}
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
