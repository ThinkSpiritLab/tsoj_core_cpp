/*
 * ExerciseUpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJobBase.hpp"
#include "logger.hpp"

extern std::ofstream log_fp;

namespace
{
	using namespace kerbal::redis;
	using namespace mysqlpp;
}

ExerciseUpdateJobBase::ExerciseUpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
}

void ExerciseUpdateJobBase::update_problem_submit(int delta)
{
	mysqlpp::Query update = mysqlConn->query(
			"update problem set p_submit = p_submit + %0 where p_id = %1"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(delta, this->pid);
	if (!res) {
		LOG_FATAL(jobType, sid, log_fp, "Update problem submit failed! ",
					"Error code: ", update.errnum(),
					", Error information: ", update.error());
		throw MysqlEmptyResException(update.errnum(), update.error());
	}
}

void ExerciseUpdateJobBase::update_problem_accept(int delta)
{
	mysqlpp::Query update = mysqlConn->query(
			"update problem set p_accept = p_accept + %0 where p_id = %1"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(delta, this->pid);
	if (!res) {
		LOG_FATAL(jobType, sid, log_fp, "Update problem p_accept failed! ",
					"Error code: ", update.errnum(),
					", Error information: ", update.error());
		throw MysqlEmptyResException(update.errnum(), update.error());
	}
}

void ExerciseUpdateJobBase::update_user_submit(int delta)
{
	mysqlpp::Query update = mysqlConn->query(
			"update user set u_submit = u_submit + %0 where u_id = %1"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(delta, uid);
	if (!res) {
		LOG_FATAL(jobType, sid, log_fp, "Update user submit failed! ",
				"Error code: ", update.errnum(),
				", Error information: ", update.error());
		throw MysqlEmptyResException(update.errnum(), update.error());
	}
}

void ExerciseUpdateJobBase::update_user_accept(int delta)
{
	mysqlpp::Query update = mysqlConn->query(
			"update user set u_accept = u_accept + %0 where u_id = %1"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(delta, uid);
	if (!res) {
		LOG_FATAL(jobType, sid, log_fp, "Update user u_accept failed! ",
				"Error code: ", update.errnum(),
				", Error information: ", update.error());
		throw MysqlEmptyResException(update.errnum(), update.error());
	}
}

void ExerciseUpdateJobBase::update_user_and_problem(const Result & result)
{
    bool already_AC = false;
    try {
    	already_AC = this->already_AC_before();
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_WARNING(jobType, sid, log_fp, "Query already ac before failed! Error information: ", e.what());
        //DO NOT THROW
        already_AC = false;
    }

    // AC后再提交不增加submit数
	if (already_AC == false) {
		this->update_user_submit(1);
		this->update_problem_submit(1);
		if (result.judge_result == UnitedJudgeResult::ACCEPTED && already_AC == false) {
			this->update_user_accept(1);
			this->update_problem_accept(1);
		}
	}
}
