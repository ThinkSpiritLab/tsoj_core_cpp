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

void CourseUpdateJob::update_user_problem(int stat)
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user_problem");
	mysqlpp::Query query = mysqlConn->query(
			"select status from user_problem "
			"where u_id = %0 and p_id = %1 and c_id = %2"
	);
	query.parse();
	mysqlpp::StoreQueryResult res = query.store(uid, pid, cid);
    if (query.errnum() != 0) {
        LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! Error information: ", query.error());
		throw MysqlEmptyResException(query.errnum(), query.error());
    }
    mysqlpp::Query insert_or_update = mysqlConn->query();
    mysqlpp::SimpleResult ret;
    if (res.empty()) {
    	insert_or_update = mysqlConn->query(
    			"insert into user_problem (u_id, p_id, c_id, status) "
                "values (%0, %1, %2, %3)"
    	);
    	insert_or_update.parse();
    	ret = insert_or_update.execute(uid, pid, cid, stat);
    } else if (stat == 0 && std::stoi(res[0][0].c_str()) != 0) {
    	insert_or_update = mysqlConn->query(
    			"update user_problem set status = %0 "
                "where u_id = %1 and p_id = %2 and c_id = %3"
    	);
		insert_or_update.parse();
    	ret = insert_or_update.execute(stat, uid, pid, cid);
    } else {
        return;
    }
    if (!ret) {
        LOG_FATAL(jobType, sid, log_fp, "Update user_problem failed! Error information: ", insert_or_update.error());
        throw MysqlEmptyResException(insert_or_update.errnum(), insert_or_update.error());
    }
}

bool CourseUpdateJob::already_AC_before()
{
    mysqlpp::Query query = mysqlConn->query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id = %2 and status = 0"
    );
    mysqlpp::StoreQueryResult res = query.store(uid, pid, cid);
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

