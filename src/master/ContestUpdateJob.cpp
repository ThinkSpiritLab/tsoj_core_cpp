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

void ContestUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_solution");
    mysqlpp::Query insert = mysqlConn->query(
            "insert into contest_solution%0 "
            "(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
            "values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
    );
    insert.parse();
    mysqlpp::SimpleResult res = insert.execute(jobType, sid, uid, pid, (int) lang, (int) result.judge_result,
                                              result.cpu_time.count(), result.memory.count(), postTime,
                                              result.similarity_percentage);
    if (!res) {
    	LOG_FATAL(jobType, sid, log_fp, "Update solution failed! Error information: ", insert.error());
        throw MysqlEmptyResException(insert.errnum(), insert.error());
    }
}

bool ContestUpdateJob::this_problem_has_not_accepted()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::this_problem_has_not_accepted");

    mysqlpp::Query query = mysqlConn->query(
            "select first_ac_user from contest_problem where ct_id = %0 and p_id = %1"
    );
    query.parse();
    mysqlpp::StoreQueryResult res = query.store(jobType, pid);

    if (res.empty()) {
    	if(query.errnum() != 0) {
    		LOG_FATAL(jobType, sid, log_fp, "Query problem's first_ac_user failed! Error information: ", query.error());
    	} else {
    		LOG_FATAL(jobType, sid, log_fp, "Check whether you have deleted the problem ", pid, " of the contest No.", jobType, " ?");
    	}
		throw MysqlEmptyResException(query.errnum(), query.error());
    }
	return res[0][0] == "NULL" ? true : false; //该字段在竞赛开始时为 NULL, 即尚未有人通过此题
}

int ContestUpdateJob::get_error_count()
{
	mysqlpp::Query query = mysqlConn->query(
			"select error_count from contest_user_problem%0 "
			"where u_id = %1 and p_id = %2"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(jobType, uid, pid);

	if (res.empty()) {
		if(query.errnum() != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Query user's error_count failed! Error information: ", query.error());
			throw MysqlEmptyResException(query.errnum(), query.error());
		}

		/*
		 * 如果查到的长度为 0, 而且查询过程也没错误, 那么就是 contest_user_problem 里没有这个数据,
		 * 亦即该用户从没提交过此题, error count 为 0
		 */
		return 0;
	} else {
		return (int) res[0][0];
	}
}

user_problem_status ContestUpdateJob::get_user_problem_status()
{
	mysqlpp::Query query = mysqlConn->query(
			"select is_ac from contest_user_problem%0 where u_id = %1 and p_id = %2"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(jobType, uid, pid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Query user's problem status failed! Error information: ", query.error());
			throw MysqlEmptyResException(query.errnum(), query.error());
		}
		return user_problem_status::TODO;
	}
	switch((int) res[0][0]) {
		case 0:
			return user_problem_status::ATTEMPTED;
		case 1:
			return user_problem_status::ACCEPTED;
		default:
			LOG_FATAL(jobType, sid, log_fp, c);
			throw std::logic_error("Undefined user's problem status!");
	}
}

void ContestUpdateJob::update_user_problem()
{
	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ? true : false;
	user_problem_status user_problem_status = this->get_user_problem_status();
    bool is_first_ac = false;

	switch (user_problem_status) {
		case user_problem_status::TODO: {
			int error_count;
			if (is_ac == true) {
				error_count = 0;
				is_first_ac = this->this_problem_has_not_accepted() == true ? true : false;
			} else {
				error_count = 1;
				is_first_ac = false;
			}
			mysqlpp::Query insert = mysqlConn->query(
					"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count, is_first_ac) "
					"values (%1, %2, %3, %4q, %5, %6)"
			);
			insert.parse();

			mysqlpp::SimpleResult res = insert.execute(jobType, uid, pid, is_ac, postTime, error_count, is_first_ac);
			break;
		}
		case user_problem_status::ATTEMPTED: {
			int error_count = this->get_error_count();

			if (is_ac == false) {
				error_count++;
				is_first_ac = false;
			} else {
				is_first_ac = this->this_problem_has_not_accepted();
			}

			mysqlpp::Query update = mysqlConn->query(
					"update contest_user_problem%0 set is_ac = %1, ac_time = %2q, error_count = %3, is_first_ac = %4 "
					"where u_id = %5 and p_id = %6"
			);
			update.parse();

			mysqlpp::SimpleResult res = update.execute(jobType, is_ac, postTime, error_count, is_first_ac, uid, pid);
			break;
		}
		case user_problem_status::ACCEPTED: {
			return;
		}
	}

    // update first_ac_user
	if (is_ac == true && is_first_ac == true) {
		mysqlpp::Query update = mysqlConn->query(
				"update contest_problem set first_ac_user = %0 where ct_id = %1 and p_id = %2"
		);

		mysqlpp::SimpleResult res = update.execute(uid, jobType, pid);

		if (!res) {
			LOG_FATAL(jobType, sid, log_fp, "update first_ac_user failed! Error information: ", update.error());
			throw MysqlEmptyResException(update.errnum(), update.error());
		}
	}
	return;
}
