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

ExerciseUpdateJob::ExerciseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
                                     std::unique_ptr <mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid,
                                                                                                    redisConn,
                                                                                                    std::move(
                                                                                                            mysqlConn))
{
}

void ExerciseUpdateJob::update_solution(const Result & result)
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

void ExerciseUpdateJob::update_user_problem(int stat)
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJob::update_user_problem");
	mysqlpp::Query query = mysqlConn->query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id is NULL"
    );
    query.parse();
    mysqlpp::StoreQueryResult res = query.store(uid, pid);
    if (query.errnum() != 0) {
        LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! Error information: ", query.error());
        throw MysqlEmptyResException(query.errnum(), query.error());
    }
    mysqlpp::Query insert_or_update = mysqlConn->query();
    mysqlpp::SimpleResult ret;
    if (res.empty()) {
        insert_or_update = mysqlConn->query(
                "insert into user_problem (u_id, p_id, status)"
                "values (%0, %1, %2)"
        );
        insert_or_update.parse();
        ret = insert_or_update.execute(uid, pid, stat);
    } else if (!stat && (int) (res[0][0]) != 0) {
        insert_or_update = mysqlConn->query(
                "update user_problem set status = %0 "
                "where u_id = %1 and p_id = %2 and c_id is NULL"
        );
        insert_or_update.parse();
        ret = insert_or_update.execute(stat, uid, pid);
    } else {
        return;
    }
    if (!ret) {
        LOG_FATAL(jobType, sid, log_fp, "Update user_problem failed! Error information: ", insert_or_update.error());
        throw MysqlEmptyResException(insert_or_update.errnum(), insert_or_update.error());
    }
}

bool ExerciseUpdateJob::already_AC_before()
{
    mysqlpp::Query query = mysqlConn->query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id is NULL and status = 0"
    );
    mysqlpp::StoreQueryResult res = query.store(uid, pid);
    bool already_AC = false;
    if (res.empty()) {
        if (query.errnum() != 0) {
            LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! ",
                      "Error code: ", query.errnum(),
                      ", Error information: ", query.error());
            throw MysqlEmptyResException(query.errnum(), query.error());
        }
    } else {
    	already_AC = true;
    }
    return already_AC;
}
