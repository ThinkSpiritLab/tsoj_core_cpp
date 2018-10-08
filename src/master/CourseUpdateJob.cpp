/*
 * CourseUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "CourseUpdateJob.hpp"
#include "logger.hpp"

extern std::ostream log_fp;


CourseUpdateJob::CourseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
		std::unique_ptr<mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
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
	mysqlpp::SimpleResult res = insert.execute(sid, uid, pid, (int) lang, (int) result.judge_result,
											(int)result.cpu_time.count(), (int)result.memory.count(), postTime, cid,
												result.similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Select status from user_problem failed!", e);
		throw e;
	}
}

void CourseUpdateJob::update_course_problem_submit(int delta)
{
	//TODO 数据库未支持记录题目提交数的字段
}

void CourseUpdateJob::update_course_problem_accept(int delta)
{
	//TODO 数据库未支持记录题目通过数的字段
}

void CourseUpdateJob::update_course_user_submit(int delta)
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_course_user_submit");

	mysqlpp::Query update = mysqlConn->query(
			"update course_user set c_submit = c_submit + %0 where c_id = %1 and u_id = %2"
	);
	update.parse();

	mysqlpp::SimpleResult res = update.execute(delta, cid, uid);

	if(!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user submit failed!", e);
		throw e;
	}
}

void CourseUpdateJob::update_course_user_accept(int delta)
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_course_user_accept");

	mysqlpp::Query update = mysqlConn->query(
			"update course_user set c_accept = c_accept + %0 where c_id = %1 and u_id = %2"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(delta, cid, uid);
	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update user u_accept failed!", e);
		throw e;
	}
}

void CourseUpdateJob::update_user_and_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::update_user_and_problem");

	// 先更新练习视角用户的提交数和通过数
	// 使 course_user 中某个 user 的提交数与通过数同步到 user 表中
	// course problem 的统计因数据库不支持暂未实现
	this->supper_t::update_user_and_problem();

	bool already_AC = false;
	try {
		if(this->get_course_user_problem_status() == user_problem_status::ACCEPTED) {
			already_AC = true;
		} else {
			already_AC = false;
		}
	} catch (const std::exception & e) {
		EXCEPT_WARNING(jobType, sid, log_fp, "Query already ac before failed!", e);
		//DO NOT THROW
		already_AC = false;
	}

	// AC后再提交不增加submit数
	if (already_AC == false) {
		this->update_course_user_submit(1);
		this->update_course_problem_submit(1);
		if (result.judge_result == UnitedJudgeResult::ACCEPTED) {
			this->update_course_user_accept(1);
			this->update_course_problem_accept(1);
		}
	}
}

user_problem_status CourseUpdateJob::get_course_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "CourseUpdateJob::get_course_user_problem_status");

	mysqlpp::Query query = mysqlConn->query(
			"select status from user_problem "
			"where u_id = %0 and p_id = %1 and c_id = %2"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(uid, pid, cid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Select status from user_problem failed!", e);
			throw e;
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

	// 先更新练习视角的用户通过情况表
	// 使课程中（cid 非空） user_problem 中某个 user 对某题的状态同步到非课程的 user_problem 中（cid 为空）
	this->supper_t::update_user_problem();

	user_problem_status old_status = this->get_course_user_problem_status();

	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ;

	switch(old_status) {
		case user_problem_status::TODO: {
			mysqlpp::Query insert = mysqlConn->query("insert into user_problem (u_id, p_id, c_id, status) "
					"values (%0, %1, %2, %3)");
			insert.parse();
			mysqlpp::SimpleResult ret = insert.execute(uid, pid, cid, is_ac ? 0 : 1);
			if (!ret) {
				MysqlEmptyResException e(insert.errnum(), insert.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "Update user_problem failed!", e);
				throw e;
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

