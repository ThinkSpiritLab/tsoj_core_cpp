/*
 * ContestUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ContestUpdateJob.hpp"
#include "boost_format_suffix.hpp"
#include "logger.hpp"

#include <kerbal/redis/redis_data_struct/list.hpp>

extern std::ostream log_fp;


ContestUpdateJob::ContestUpdateJob(int jobType, int sid, const RedisContext & redisConn,
		std::unique_ptr <mysqlpp::Connection> && mysqlConn) : supper_t(jobType, sid, redisConn, std::move(mysqlConn))
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::ContestUpdateJob");
}

void ContestUpdateJob::update_solution()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_solution");

	// 每个 solution 之前均不存在，使用 insert
	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_solution%0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, uid, pid, (int) lang, (int) result.judge_result,
												(int)result.cpu_time.count(), (int)result.memory.count(), postTime,
												result.similarity_percentage);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update solution failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_source_code(const char * source_code)
{
	if (source_code == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty source code!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_source%0 (s_id, source_code)"
			"values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, source_code);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update source code failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_compile_info(const char * compile_info)
{
	if (compile_info == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty compile info!");
		return;
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into contest_compile_info%0 (s_id, compile_error_info) values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(jobType, sid, compile_info);
	if (!res) {
		MysqlEmptyResException e(insert.errnum(), insert.error());
		EXCEPT_FATAL(jobType, sid, log_fp, "Update compile info failed!", e);
		throw e;
	}
}

void ContestUpdateJob::update_problem_submit_and_accept_num_in_this_contest()
{
	std::set<int> accepted_users;
	int submit_num = 0;
	{
		mysqlpp::Query query = mysqlConn->query(
				"select u_id, s_result from contest_solution%0 where p_id = %1 order by s_id"
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(this->jobType, this->pid);

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
				"update contest_problem set ct_p_submit = %0, ct_p_accept = %1 where p_id = %2 and ct_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(submit_num, accepted_users.size(), this->pid, this->jobType);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Update problem's submit and accept number in this contest failed!", e, ", contest id: ", this->jobType);
			throw e;
		}
	}
}

void ContestUpdateJob::update_user_and_problem()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user_and_problem");

	try {
		this->update_problem_submit_and_accept_num_in_this_contest();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Update problem submit and accept number in this contest failed!", e, ", contest id: ", this->jobType);
		throw;
	}

	// 竞赛不需更新用户提交数通过数
}

int ContestUpdateJob::get_error_count()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::get_error_count");

	// 本题的非 AC 次数，并不是所有的非 AC 次数
	mysqlpp::Query query = mysqlConn->query(
			"select error_count from contest_user_problem%0 "
			"where u_id = %1 and p_id = %2"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(jobType, uid, pid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query user's error_count failed!", e);
			throw e;
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

user_problem_status ContestUpdateJob::get_contest_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::get_contest_user_problem_status");

	mysqlpp::Query query = mysqlConn->query(
			"select is_ac from contest_user_problem%0 where u_id = %1 and p_id = %2"
	);
	query.parse();

	mysqlpp::StoreQueryResult res = query.store(jobType, uid, pid);

	if (res.empty()) {
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query user's problem status failed!", e);
			throw e;
		}
		/*
		 * 如果查到的长度为 0, 而且查询过程也没错误, 那么就是 contest_user_problem 里没有这个数据,
		 * 亦即该用户从没提交过此题, 状态为 TODO
		 */
		return user_problem_status::TODO;
	}

	// 注意这里 is_ac 字段 0 1 的含义与练习中的 status 字段是反的
	switch((int) res[0][0]) {
		case 0: // is_ac 字段为 0 表示尝试过,
			return user_problem_status::ATTEMPTED;
		case 1: // is_ac 字段为 1 表示已 AC
			return user_problem_status::ACCEPTED;
		default:
			LOG_FATAL(jobType, sid, log_fp, "Undefined user's problem status!");
			throw std::logic_error("Undefined user's problem status!");
	}
}

void ContestUpdateJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user_problem");

	if (this->result.judge_result == UnitedJudgeResult::SYSTEM_ERROR) { // ignore system error
		return;
	}

	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ? true : false;

	// get_contest_user_problem_status 有可能出错而抛出异常，因此用 try-catch 语句块。这导致了 user_problem_status
	// 需定义在 try-catch 语句块之外，否则因为可见性的缘故，本函数的后续部分无法访问到它。其余部分同理。
	// user_problem_status 为不考虑本次 solution 的之前状态。
	user_problem_status old_status = user_problem_status::TODO;
	try {
		old_status = this->get_contest_user_problem_status();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Get user problem status failed!", e);
		throw;
	}

	switch (old_status) {
		case user_problem_status::TODO: {
			int error_count = 0;
			if (is_ac == false) {
				error_count++;
			}
			mysqlpp::Query insert = mysqlConn->query(
					"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
					"values (%1, %2, %3, %4q, %5)"
			);
			insert.parse();

			mysqlpp::SimpleResult res = insert.execute(jobType, uid, pid, is_ac, postTime, error_count);
			if (!res) {
				MysqlEmptyResException e(insert.errnum(), insert.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "insert user problem status failed!", e);
				throw e;
			}
			break;
		}
		case user_problem_status::ATTEMPTED: {
			int error_count = this->get_error_count();

			if (is_ac == false) {
				error_count++;
			}

			mysqlpp::Query update = mysqlConn->query(
					"update contest_user_problem%0 set is_ac = %1, ac_time = %2q, error_count = %3 "
					"where u_id = %4 and p_id = %5"
			);
			update.parse();

			mysqlpp::SimpleResult res = update.execute(jobType, is_ac, postTime, error_count, uid, pid);
			if (!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "update user problem status failed!", e);
				throw e;
			}
			break;
		}
		case user_problem_status::ACCEPTED:
			break;
	}

	try {
		kerbal::redis::List<int> update_contest_scoreboard_queue(this->redisConn, "update_contest_scoreboard_queue");
		update_contest_scoreboard_queue.push_back(jobType);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Insert update scoreboard job failed!", e);
		return;
	}
	return;
}
