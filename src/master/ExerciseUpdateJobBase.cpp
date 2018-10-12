/*
 * ExerciseUpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ExerciseUpdateJobBase.hpp"
#include "logger.hpp"

extern std::ostream log_fp;


ExerciseUpdateJobBase::ExerciseUpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJobBase::ExerciseUpdateJobBase");
}

void ExerciseUpdateJobBase::update_source_code(const char * source_code)
{
	if (source_code == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty source code!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into source (s_id, source_code)"
			"values (%0, %1q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(sid, source_code);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update source code failed!", e);
		throw e;
	}
}

void ExerciseUpdateJobBase::update_compile_info(const char * compile_info)
{
	if (compile_info == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty compile info!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into compile_info (s_id, compile_error_info) values (%0, %1q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(sid, compile_info);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update compile info failed!", e);
		throw e;
	}
}

void ExerciseUpdateJobBase::update_problem_submit_and_accept_num_in_exercise()
{
	mysqlpp::Query query = mysqlConn->query(
			"select u_id, s_result from solution where p_id = %0 order by s_id"
			// 这里无需添加 c_id is null 的条件, 因为用户在课程中的一条提交也是被当作一条在练习中的提交处理的
	);
	query.parse();

	std::set<int> accepted_user;
	int submit_num = 0;
	{
		mysqlpp::StoreQueryResult res = query.store(this->pid);

		for (const auto & row : res) {
			int u_id_in_this_row = row["u_id"];
			// 此题已通过的用户的集合中无此条 solution 对应的用户
			if (accepted_user.find(u_id_in_this_row) == accepted_user.end()) {
				UnitedJudgeResult result = UnitedJudgeResult(int(row["s_result"]));
				if (result == UnitedJudgeResult::ACCEPTED) {
					accepted_user.insert(u_id_in_this_row);
				}
				++submit_num;
			}
		}

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query problem's solutions failed!", e);
			throw e;
		}
	}

	mysqlpp::Query update = mysqlConn->query(
			"update problem set p_submit = %0, p_accept = %1 where p_id = %2"
	);
	update.parse();

	mysqlpp::SimpleResult res = update.execute(submit_num, accepted_user.size(), this->pid);
	if (!res) {
		MysqlEmptyResException e(query.errnum(), query.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update problem's submit and accept number in exercise failed!", e);
		throw e;
	}
}

void ExerciseUpdateJobBase::update_user_submit_and_accept_num_in_exercise()
{
	mysqlpp::Query query = mysqlConn->query(
			"select p_id, s_result from solution where u_id = %0 order by s_id"
			// 这里无需添加 c_id is null 的条件, 因为用户在课程中的一条提交也是被当作一条在练习中的提交处理的
	);
	query.parse();

	std::set<int> accepted_problems;
	int submit_num = 0;
	{
		mysqlpp::StoreQueryResult res = query.store(this->uid);

		for (const auto & row : res) {
			int p_id_in_this_row = row["p_id"];
			if (accepted_problems.find(p_id_in_this_row) == accepted_problems.end()) {
				// 此用户已通过的题的集合中无此条 solution 对应的题
				UnitedJudgeResult result = UnitedJudgeResult(int(row["s_result"]));
				if (result == UnitedJudgeResult::ACCEPTED) {
					accepted_problems.insert(p_id_in_this_row);
				}
				++submit_num;
			}
		}

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query user's solutions failed!", e);
			throw e;
		}
	}

	mysqlpp::Query update = mysqlConn->query(
			"update user set u_submit = %0, u_accept = %1 where u_id = %2"
	);
	update.parse();

	mysqlpp::SimpleResult res = update.execute(submit_num, accepted_problems.size(), this->uid);
	if (!res) {
		MysqlEmptyResException e(query.errnum(), query.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user's submit and accept number in exercise failed!", e);
		throw e;
	}
}

void ExerciseUpdateJobBase::update_user_and_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJobBase::update_user_and_problem");

	try {
		this->update_user_submit_and_accept_num_in_exercise();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user submit and accept number in exercise failed!", e);
		throw;
	}

	try {
		this->update_problem_submit_and_accept_num_in_exercise();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update problem submit and accept number in exercise failed!", e);
		throw;
	}
}

user_problem_status ExerciseUpdateJobBase::get_exercise_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJobBase::get_exercise_user_problem_status");

	mysqlpp::Query query = mysqlConn->query(
		"select status from user_problem "
		"where u_id = %0 and p_id = %1 and c_id is NULL"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(uid, pid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query user problem status failed!", e);
			throw e;
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

void ExerciseUpdateJobBase::update_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ExerciseUpdateJobBase::update_user_problem");

	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ? true : false;
	user_problem_status old_status = this->ExerciseUpdateJobBase::get_exercise_user_problem_status();
	switch (old_status) {
		case user_problem_status::TODO: {
			// 未做过此题时，user_problem 表中无记录，使用 insert
			mysqlpp::Query insert = mysqlConn->query(
					"insert into user_problem (u_id, p_id, status)"
					"values (%0, %1, %2)"
			);
			insert.parse();
			mysqlpp::SimpleResult ret = insert.execute(uid, pid, is_ac == true ? 0 : 1);
			if (!ret) {
				MysqlEmptyResException e(insert.errnum(), insert.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user_problem failed!", e);
				throw e;
			}
			break;
		}
		case user_problem_status::ATTEMPTED: {
			// 尝试做过此题时，user_problem 表中有记录，使用 update
			mysqlpp::Query update = mysqlConn->query(
					"update user_problem set status = %0 "
					"where u_id = %1 and p_id = %2 and c_id is NULL"
			);
			update.parse();
			mysqlpp::SimpleResult ret = update.execute(is_ac == true ? 0 : 1, uid, pid);
			if (!ret) {
				MysqlEmptyResException e(update.errnum(), update.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user_problem failed!", e);
				throw e;
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

