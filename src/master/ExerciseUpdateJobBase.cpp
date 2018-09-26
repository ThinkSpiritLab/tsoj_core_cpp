/*
 * ExerciseUpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJobBase.hpp"
#include "logger.hpp"

extern std::ostream log_fp;

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
										"Error code: ", update.errnum(), ", "
										"Error information: ", update.error());
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
										"Error code: ", update.errnum(), ", "
										"Error information: ", update.error());
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
										"Error code: ", update.errnum(), ", "
										"Error information: ", update.error());
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
										"Error code: ", update.errnum(), ", "
										"Error information: ", update.error());
		throw MysqlEmptyResException(update.errnum(), update.error());
	}
}

void ExerciseUpdateJobBase::update_user_and_problem()
{
	bool already_AC = false;
	try {
		if(this->get_user_problem_status() == user_problem_status::ACCEPTED) {
			already_AC = true;
		} else {
			already_AC = false;
		}
	} catch (const mysqlpp::BadParamCount & e) {
		LOG_WARNING(jobType, sid, log_fp, "Query already ac before failed! Error information: ", e.what());
		//DO NOT THROW
		already_AC = false;
	}

	// AC后再提交不增加submit数
	if (already_AC == false) {
		this->update_user_submit(1);
		this->update_problem_submit(1);
		if (result.judge_result == UnitedJudgeResult::ACCEPTED) {
			this->update_user_accept(1);
			this->update_problem_accept(1);
		}
	}
}

user_problem_status ExerciseUpdateJobBase::get_user_problem_status()
{
	mysqlpp::Query query = mysqlConn->query(
		"select status from user_problem "
		"where u_id = %0 and p_id = %1 and c_id is NULL"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(uid, pid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Query user problem status failed! ",
											"Error code: ", query.errnum(), ", "
											"Error information: ", query.error());
			throw MysqlEmptyResException(query.errnum(), query.error());
		}
		return user_problem_status::TODO;
	}

	switch ((int) res[0][0]) {
		case 0:
			return user_problem_status::ACCEPTED;
		case 1:
			return user_problem_status::ATTEMPTED;
		default:
			LOG_FATAL(jobType, sid, log_fp, "Undefined user's problem status!");
			throw std::logic_error("Undefined user's problem status!");
	}
}

void ExerciseUpdateJobBase::update_user_problem()
{
	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ? true : false;
	user_problem_status old_status = this->get_user_problem_status();
	switch (old_status) {
		case user_problem_status::TODO: {
			mysqlpp::Query insert = mysqlConn->query(
					"insert into user_problem (u_id, p_id, status)"
					"values (%0, %1, %2)"
			);
			insert.parse();
			mysqlpp::SimpleResult ret = insert.execute(uid, pid, is_ac == true ? 0 : 1);
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
					"where u_id = %1 and p_id = %2 and c_id is NULL"
			);
			update.parse();
			mysqlpp::SimpleResult ret = update.execute(is_ac == true ? 0 : 1, uid, pid);
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

