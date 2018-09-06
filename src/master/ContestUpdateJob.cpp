/*
 * ContestUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ContestUpdateJob.hpp"
#include "boost_format_suffix.hpp"
#include "logger.hpp"

namespace
{
    using namespace kerbal::redis;
    using namespace mysqlpp;
}

extern std::ostream log_fp;

ContestUpdateJob::ContestUpdateJob(int jobType, int sid, const RedisContext & redisConn,
           std::unique_ptr <mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::ContestUpdateJob");
}

void ContestUpdateJob::update_solution(const Result & result)
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_solution");
    mysqlpp::Query insert = mysqlConn->query(
            "insert into %0 "
            "(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
            "values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
    );
    insert.parse();
    std::string solution_table = "contest_solution%d"_fmt(jobType).str();
    mysqlpp::SimpleResult res = insert.execute(solution_table, sid, uid, pid, (int) lang, (int) result.judge_result,
                                              result.cpu_time.count(), result.memory.count(), postTime,
                                              result.similarity_percentage);
    if (!res) {
        throw MysqlEmptyResException(insert.errnum(), insert.error());
    }
}

bool ContestUpdateJob::is_first_ac_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::is_first_ac_user");
    mysqlpp::Query query = mysqlConn->query(
            "select first_ac_user from contest_problem where ct_id = %0 and p_id = %1"
    );
    query.parse();
    mysqlpp::StoreQueryResult res = query.store(jobType, pid);

    if (res.empty()) {
        LOG_FATAL(jobType, sid, log_fp, "Query first_ac_user failed! Error information: ", query.error());
        LOG_WARNING(jobType, sid, log_fp, "Check whether you have deleted the problem ", pid, " of the contest No.",
                    jobType, " ?");
        throw MysqlEmptyResException(query.errnum(), query.error());
    }
	return res[0][0] == "NULL" ? true : false; //该字段在竞赛开始时为 NULL, 即尚未有人通过此题
}

void ContestUpdateJob::update_user_problem(int stat)
{
    bool is_first_ac = this->is_first_ac_solution();

    // is already AC
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select is_ac from contest_user_problem%d where u_id = %d and p_id = %d", jobType, uid, pid);

    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "select is_ac from contest_user_problem%d failed!, errno:%d, error:%s\nsql:%s", jobType,
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "store result failed! res is NULL, errno:%d, error:%s\nsql:%s",
                  mysql_errno(conn), mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);

    if (row == NULL) {
        memset(sql, 0, sizeof(sql));
        int error_count;
        if (is_ac) {
            error_count = 0;
        } else {
            error_count = 1;
            is_first_ac = 0;
        }
        sprintf(sql, "insert into contest_user_problem%d (u_id, p_id, is_ac, ac_time, error_count, is_first_ac) "
                     "values ('%d', '%d', '%d', '%s', '%d', '%d')", jobType, uid, pid, is_ac, postTime.c_str(),
                error_count, is_first_ac);

    } else if (atoi(row[0]) == 0) {
        // get error_count
        MYSQL_RES * tmp_res;
        MYSQL_ROW tmp_row;
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "select error_count from contest_user_problem%d "
                     "where u_id = %d and p_id = %d", jobType, uid, pid);
        int tmp_ret = mysql_real_query(conn, sql, strlen(sql));
        if (tmp_ret) {
            LOG_WARNING(jobType, sid, log_fp, "select status from user_problem failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return tmp_ret;
        }
        tmp_res = mysql_store_result(conn);
        if (tmp_res == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                      mysql_error(conn), sql);
            return mysql_errno(conn);
        }
        tmp_row = mysql_fetch_row(tmp_res);
        if (tmp_row == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "impossible! get error_count failed!, errno:%d, error:%s\nsql:%s",
                      mysql_errno(conn), mysql_error(conn), sql);
            return -1;
        }
        int error_count = atoi(tmp_row[0]);
        mysql_free_result(tmp_res);

        if (!is_ac) {
            error_count++;
            is_first_ac = 0;
        }

        memset(sql, 0, sizeof(sql));
        sprintf(sql, "update contest_user_problem%d set is_ac = %d, ac_time = '%s', error_count = %d, is_first_ac = %d "
                     "where u_id = %d and p_id = %d", jobType, is_ac, postTime.c_str(), error_count, is_first_ac, uid,
                pid);
    } else {
        mysql_free_result(res);
        return 0;
    }

    mysql_free_result(res);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "update contest_user_problem%d failed!, errno:%d, error:%s\nsql:%s", jobType,
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }

    // update first_ac_user
    if (is_ac && is_first_ac) {
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "update contest_problem set first_ac_user = %d where ct_id = %d and p_id = %d", uid, jobType, pid);

        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp, "update first_ac_user failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return ret;
        }
    }

    return 0;
}
