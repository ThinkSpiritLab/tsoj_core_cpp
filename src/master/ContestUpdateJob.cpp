/*
 * ContestUpdateJob.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "ContestUpdateJob.hpp"
#include "boost_format_suffix.hpp"
#include "logger.hpp"

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

void ContestUpdateJob::update_user_and_problem()
{
	//TODO SQL 里没有对应的表结构, 先设为空函数体
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

bool ContestUpdateJob::this_problem_has_not_accepted()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::this_problem_has_not_accepted");

	mysqlpp::Query query = mysqlConn->query(
			"select first_ac_user from contest_problem where ct_id = %0 and p_id = %1"
	);
	query.parse();
	mysqlpp::StoreQueryResult res = query.store(jobType, pid);

	// 该字段在竞赛题目建立时被创建，因此无论如何都能够查询到该字段，否则出现逻辑错误
	if (res.empty()) {
		if(query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "Query problem's first_ac_user failed!", e);
			throw e;
		} else {
			std::logic_error e("Check whether you have deleted the problem");
			LOG_FATAL(jobType, sid, log_fp, "Check whether you have deleted the problem ", pid, " of the contest No.", jobType, " ?");
			throw e;
		}
	}
	return res[0][0] == "NULL" ? true : false; //该字段在竞赛开始时为 NULL, 即尚未有人通过此题
}


void ContestUpdateJob::update_user_problem_status()
{
	LOG_DEBUG(jobType, sid, log_fp, "ContestUpdateJob::update_user_problem");

	bool is_ac = this->result.judge_result == UnitedJudgeResult::ACCEPTED ? true : false;

	// get_contest_user_problem_status 有可能出错而抛出异常，因此用 try-catch 语句块。这导致了 user_problem_status
	// 需定义在 try-catch 语句块之外，否则因为可见性的缘故，本函数的后续部分无法访问到它。其余部分同理。
	// user_problem_status 为不考虑本次 solution 的之前状态。
	user_problem_status user_problem_status = user_problem_status::TODO;
	try {
		user_problem_status = this->get_contest_user_problem_status();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Get user problem status failed!", e);
		throw;
	}

	bool is_first_ac = false;
	if (is_ac == true) {
		try {
			is_first_ac = this->this_problem_has_not_accepted() == true ? true : false;
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, sid, log_fp, "Query is first ac failed!", e);
			throw;
		}
	}

	switch (user_problem_status) {
		case user_problem_status::TODO: {
			int error_count = 0;
			if (is_ac == false) {
				error_count++;
			}
			mysqlpp::Query insert = mysqlConn->query(
					"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count, is_first_ac) "
					"values (%1, %2, %3, %4q, %5, %6)"
			);
			insert.parse();

			mysqlpp::SimpleResult res = insert.execute(jobType, uid, pid, is_ac, postTime, error_count, is_first_ac);
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
					"update contest_user_problem%0 set is_ac = %1, ac_time = %2q, error_count = %3, is_first_ac = %4 "
					"where u_id = %5 and p_id = %6"
			);
			update.parse();

			mysqlpp::SimpleResult res = update.execute(jobType, is_ac, postTime, error_count, is_first_ac, uid, pid);
			if (!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				EXCEPT_FATAL(jobType, sid, log_fp, "update user problem status failed!", e);
				throw e;
			}
			break;
		}
		case user_problem_status::ACCEPTED:
			return;
	}

	// Q: 众所周知，slave 端是多进程进行判题的，若是由于系统调度的缘故，先提交的那个人的评测在后提交的人之后完成，这样
	//    update 的队列中，后提交的人的更新任务在前。那么在这里修改 first_AC 的时候逻辑会出现问题。
	//    个人能想到的是，所以是否可以去掉 is_first_ac 字段，并不在更新每一次提交的时候都检查一次，而是在 master 开始
	//    评测任务之前，开启一个子进程，每隔一段时间（比如30秒），查询一次数据库，排序出提交时间最早的用户来更新 
	//    contest_problem 表中的 first_ac_user 字段。
	// update first_ac_user
	if (is_ac == true && is_first_ac == true) {
		mysqlpp::Query update = mysqlConn->query(
				"update contest_problem set first_ac_user = %0 where ct_id = %1 and p_id = %2"
		);
		update.parse();
		mysqlpp::SimpleResult res = update.execute(uid, jobType, pid);

		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(jobType, sid, log_fp, "update first_ac_user failed!", e);
			throw e;
		}
	}
	return;
}
