/*
 * CourseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "CourseUpdateJob.hpp"
#include "logger.hpp"
#include "CourseManagement.hpp"
#include "ExerciseManagement.hpp"

extern std::ofstream log_fp;


CourseUpdateJob::CourseUpdateJob(int jobType, int sid, ojv4::c_id_type c_id, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
				supper_t(jobType, sid, redisConn, std::move(mysqlConn)),
				c_id(c_id)
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::CourseUpdateJob");
}

void CourseUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_solution");

	mysqlpp::Query insert = mysqlConn->query(
			"insert into solution "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, c_id, s_similarity_percentage)"
			"values (%0, %1, %2, %3, %4, %5, %6, %7q, %8, %9)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(sid, u_id, pid, (int) lang, (int) result.judge_result,
											(int)result.cpu_time.count(), (int)result.memory.count(), s_posttime, c_id,
												similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		throw e;
	}
}

void CourseUpdateJob::update_problem_submit_and_accept_num_in_this_course()
{
	std::set<int> accepted_users;
	int submit_num = 0;

	{
		mysqlpp::Query query = mysqlConn->query(
				"select u_id, s_result from solution where p_id = %0 and c_id = %1 order by s_id"
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(this->pid, this->c_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query problem's solutions failed!", e);
			throw e;
		}

		for (const auto & row : res) {
			int u_id_in_this_row = row["u_id"];
			// 此题已通过的用户的集合中无此条 solution 对应的用户
			if (accepted_users.find(u_id_in_this_row) == accepted_users.end()) {
				UnitedJudgeResult result = UnitedJudgeResult(int(row["s_result"]));
				switch(result) {
					case UnitedJudgeResult::ACCEPTED:
						accepted_users.insert(u_id_in_this_row);
						++submit_num;
						break;
					case UnitedJudgeResult::SYSTEM_ERROR: // ignore system error
						break;
					default:
						++submit_num;
						break;
				}
			}
		}
	}

	{
		mysqlpp::Query update = mysqlConn->query(
				"update course_problem set c_p_submit = %0, c_p_accept = %1 where p_id = %2 and c_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(submit_num, accepted_users.size(), this->pid, this->c_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Update problem's submit and accept number in this course failed!", e, ", course id: ", this->c_id);
			throw e;
		}
	}
}

void CourseUpdateJob::update_user_submit_and_accept_num_in_this_course()
{
	std::set<int> accepted_problems;
	int submit_num = 0;

	{
		mysqlpp::Query query = mysqlConn->query(
				"select p_id, s_result from solution where u_id = %0 and c_id = %1 order by s_id"
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(this->uid, this->c_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query user's solutions failed!", e);
			throw e;
		}

		for (const auto & row : res) {
			int p_id_in_this_row = row["p_id"];
			if (accepted_problems.find(p_id_in_this_row) == accepted_problems.end()) {
				// 此用户已通过的题的集合中无此条 solution 对应的题
				UnitedJudgeResult result = UnitedJudgeResult(int(row["s_result"]));
				switch(result) {
					case UnitedJudgeResult::ACCEPTED:
						accepted_problems.insert(p_id_in_this_row);
						++submit_num;
						break;
					case UnitedJudgeResult::SYSTEM_ERROR: // ignore system error
						break;
					default:
						++submit_num;
						break;
				}
			}
		}
	}

	{
		mysqlpp::Query update = mysqlConn->query(
				"update course_user set c_submit = %0, c_accept = %1 where u_id = %2 and c_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(submit_num, accepted_problems.size(), this->uid, this->c_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Update user's submit and accept number in this course failed!", e, ", course id: ", this->c_id);
			throw e;
		}
	}
}


void CourseUpdateJob::update_user()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user");

	try {
		CourseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->c_id, this->u_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-user's submit and accept number failed!", e, ", c_id: ", this->c_id);
		throw;
	}

	// 更新练习视角用户的提交数和通过数
	// 使课程中某个 user 的提交数与通过数同步到 user 表中
	try {
		ExerciseManagement::update_user_s_submit_and_accept_num(*this->mysqlConn, this->u_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-user's submit and accept number in exercise view failed!", e, ", c_id: ", this->c_id);
		throw;
	}
}

void CourseUpdateJob::update_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_problem");

	try {
		CourseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->c_id, this->u_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-problem's submit and accept number failed!", e, ", c_id: ", this->c_id);
		throw;
	}

	// 更新练习视角题目的提交数和通过数
	// 使课程中某个 problem 的提交数与通过数同步到 problem 表中
	try {
		ExerciseManagement::update_problem_s_submit_and_accept_num(*this->mysqlConn, this->u_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-problem's submit and accept number in exercise view failed!", e, ", c_id: ", this->c_id);
		throw;
	}
}


void CourseUpdateJob::update_user_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user_problem");

	try {
		CourseManagement::update_user_problem(*this->mysqlConn, this->c_id, this->u_id, this->pid);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user-problem failed!", e, "c_id: ", this->c_id);
		throw;
	}

	// 更新练习视角的用户通过情况表
	try {
//		ExerciseManagement::up
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user-problem in exercise view failed!", e, "c_id: ", this->c_id);
		throw;
	}
}

void CourseUpdateJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user_problem_status");

	try {
		CourseManagement::update_user_problem_status(*this->mysqlConn, this->c_id, this->u_id, this->pid);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-user's problem status failed!", e, "c_id: ", this->c_id);
		throw;
	}

	// 更新练习视角的用户通过情况表
	try {
//		ExerciseManagement::up
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update course-user's problem status in exercise view failed!", e, "c_id: ", this->c_id);
		throw;
	}

}

