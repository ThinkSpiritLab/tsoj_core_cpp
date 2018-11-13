/*
 * CourseRejudgeUpdateJob.cpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#include "CourseRejudgeUpdateJob.hpp"
#include "logger.hpp"

extern std::ofstream log_fp;

CourseRejudgeUpdateJob::CourseRejudgeUpdateJob(int jobType, int sid, int cid, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
				CourseUpdateJob(jobType, sid, cid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseRejudgeUpdateJob::CourseRejudgeUpdateJob");
}

void CourseRejudgeUpdateJob::move_orig_solution_to_rejudge_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseRejudgeUpdateJob::move_orig_solution_to_rejudge_solution");

	mysqlpp::Query query = mysqlConn->query(
			"select s_result, s_time, s_mem, s_similarity_percentage "
			"from solution where s_id = %0"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(this->sid);

	if (res.empty()) {
		throw MysqlEmptyResException(query.errnum(), query.error());
	}

	const auto & row = res[0];
	ojv4::s_result_enum_type orig_result = row["s_result"];
	ojv4::s_time_in_milliseconds orig_time = row["s_time"];
	ojv4::s_mem_in_byte orig_mem = row["s_mem"];
	ojv4::s_similarity_type orig_similarity = row["s_similarity_percentage"];

	mysqlpp::Query insert()
}

void CourseRejudgeUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseRejudgeUpdateJob::update_solution");

    mysqlpp::Query update = mysqlConn->query(
            "update solution"
            "set s_result = %0, s_time = %1, s_mem = %2, s_similarity_percentage = %3 "
            "where s_id = %4"
    );
    update.parse();
	mysqlpp::SimpleResult res = update.execute((int) result.judge_result, (int)result.cpu_time.count(),
												(int)result.memory.count(), similarity_percentage, this->sid);


	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		throw e;
	}
}
