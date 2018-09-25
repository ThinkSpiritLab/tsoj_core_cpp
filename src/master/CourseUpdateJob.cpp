/*
 * CourseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "logger.hpp"
#include "CourseUpdateJob.hpp"

extern std::ostream log_fp;

namespace
{
	using namespace kerbal::redis;
	using namespace mysqlpp;
}

CourseUpdateJob::CourseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::CourseUpdateJob");
}

void CourseUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_solution");

    std::string solution_table = "solution";

    mysqlpp::Query templ = mysqlConn->query(
            "insert into %0 "
            "(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, c_id, s_similarity_percentage)"
            "values (%1, %2, %3, %4, %5, %6, %7, %8q, %9, %10)"
    );
    templ.parse();
    mysqlpp::SimpleResult res = templ.execute(solution_table, sid, uid, pid, (int) lang, (int) result.judge_result,
                                              result.cpu_time.count(), result.memory.count(), postTime, cid,
                                              result.similarity_percentage);
    if (!res) {
        throw MysqlEmptyResException(templ.errnum(), templ.error());
    }
}

void CourseUpdateJob::update_user_and_problem()
{
	//TODO
}

user_problem_status CourseUpdateJob::get_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::get_user_problem_status");

    mysqlpp::Query query = mysqlConn->query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id = %2"
    );
    query.parse();

    mysqlpp::StoreQueryResult res = query.store(uid, pid, cid);

    if (res.empty()) {
        if (query.errnum() != 0) {
            LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! ",
											  "Error code: ", query.errnum(), ", ",
											  "Error information: ", query.error());
            throw MysqlEmptyResException(query.errnum(), query.error());
        }
        return user_problem_status::TODO;
    }
    switch((int) res[0][0]) {
		case 0:
			return user_problem_status::ACCEPTED;
		case 1:
			return user_problem_status::ATTEMPTED;
		default:
			LOG_FATAL(jobType, sid, log_fp, "Undefined user's problem status!");
			throw std::logic_error("Undefined user's problem status!");
	}
}

void CourseUpdateJob::update_user_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user_problem");

	user_problem_status old_status = this->get_user_problem_status();

	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ;

	switch(old_status) {
		case user_problem_status::TODO: {
			mysqlpp::Query insert = mysqlConn->query("insert into user_problem (u_id, p_id, c_id, status) "
					"values (%0, %1, %2, %3)");
			insert.parse();
			mysqlpp::SimpleResult ret = insert.execute(uid, pid, cid, is_ac ? 0 : 1);
			if (!ret) {
				LOG_FATAL(jobType, sid, log_fp, "Update user_problem failed! ",
												"Error code: ", insert.errnum(), ", ",
												"Error information: ", insert.error());
				throw MysqlEmptyResException(insert.errnum(), insert.error());
			}
			break;
		}
		case user_problem_status::ATTEMPTED: {
			mysqlpp::Query update = mysqlConn->query(
					"update user_problem set status = %0 "
					"where u_id = %1 and p_id = %2 and c_id = %3"
			);
			update.parse();
			mysqlpp::SimpleResult ret = update.execute(is_ac ? 0 : 1, uid, pid, cid);
			if (!ret) {
				LOG_FATAL(jobType, sid, log_fp, "Update user_problem failed! ",
												"Error code: ", update.errnum(), ", ",
												"Error information: ", update.error());
				throw MysqlEmptyResException(update.errnum(), update.error());
			}
			break;
		}
		case user_problem_status::ACCEPTED:
			return;
		default:
			LOG_FATAL(jobType, sid, log_fp, "Undefined user's problem status!");
			throw std::logic_error("Undefined user's problem status!");
	}
}

