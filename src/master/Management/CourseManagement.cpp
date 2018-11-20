/*
 * CourseManagement.cpp
 *
 *  Created on: 2018年11月11日
 *      Author: peter
 */

#include "CourseManagement.hpp"
#include "mysql_empty_res_exception.hpp"
#include "logger.hpp"
#include "sync_instance_pool.hpp"
#include "SARecorder.hpp"

#include <unordered_set>
#include <unordered_map>
#include <future>
#include <mutex>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>


extern std::ofstream log_fp;


void CourseManagement::refresh_all_users_submit_and_accept_num_in_course(ojv4::c_id_type c_id)
{
	// 以下课程/考试的成绩较为敏感, 其中用户的提交数通过数不应当再刷新. 故使用硬编码的方式予以保护
	static const std::unordered_set<ojv4::c_id_type, id_type_hash<ojv4::c_id_type>> protected_course = {

		16_c_id, // 2017级程序设计实验/实训
		17_c_id, // 2018研究生复试
		22_c_id, // 2017级程序设计实验/实训期末考试
		23_c_id, // 2017级程序设计实验/实训补考
	};

	if(protected_course.find(c_id) != protected_course.cend()) {
		LOG_WARNING(0, 0, log_fp, "Protected course/exam. c_id: ", c_id);
		return;
	}

	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	auto conn = conn_pool::fetch();

	mysqlpp::Query query = conn->query(
			"select u_id, p_id, s_result from solution "
			"where c_id = %0 and p_id in (select p_id from course_problem where c_id = %1) order by s_id"
			// 注: 需要过滤掉不在课程中的题目 (考虑到有加题再删题的情形)
			// 思考: 需不需要过滤掉不在课程中的学生呢? 答案在下面若干行
			// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
	);
	query.parse();

	std::unordered_map<ojv4::u_id_type, UserSARecorder, id_type_hash<ojv4::u_id_type>> m;
	{
		mysqlpp::StoreQueryResult solutions = query.store(c_id, c_id);
		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			m[u_id].add_solution(p_id, s_result);
		}
	}

	std::vector<std::pair<ojv4::u_id_type, std::future<mysqlpp::SimpleResult>>> future_group;

	for (const auto & ele : m) {
		ojv4::u_id_type u_id = ele.first;
		const UserSARecorder & u_info = ele.second;
		int submit = u_info.submit_num();
		int accept = u_info.accept_num();

		auto as = [](conn_pool::auto_revert_t conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, int submit, int accept) -> mysqlpp::SimpleResult
		{
			mysqlpp::Query update = conn->query(
					"update course_user set c_submit = %2, c_accept = %3 "
					"where c_id = %0 and u_id = %1"
			);
			update.parse();
			mysqlpp::SimpleResult res = update.execute(c_id, u_id, submit, accept);
			if(!res) {
				/*
				 * 注: 对于不在课程中的学生, update 不会出错, 不会执行到该分支
				 * 提示: 假设你在命令行中模拟执行该 update 语句, 会提示
				 * Query OK, 0 rows affected (0.01 sec)
				 * Rows matched: 0  Changed: 0  Warnings: 0
				 * where 子句不会匹配到相应的行, 也就不会出错了
				 */
				MysqlEmptyResException e(update.errnum(), update.error());
				throw e;
			}
			return res;
		};
		conn_pool::auto_revert_t conn = conn_pool::block_fetch();

		future_group.emplace_back(u_id, std::async(std::launch::async, as, std::move(conn), c_id, u_id, submit, accept));
	}

	std::exception_ptr e_ptr;
	for(auto & ele : future_group) {
		ojv4::u_id_type u_id = ele.first;
		try {
			mysqlpp::SimpleResult res = ele.second.get();
		} catch(const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Refresh all users' submit and accept num in course failed!", e, " c_id: ", c_id, " u_id: ", u_id);
			if(e_ptr == nullptr) {
				// 保存首个异常
				e_ptr = std::current_exception();
			}
		}
	}
	if(e_ptr != nullptr) {
		std::rethrow_exception(e_ptr);
	}
}

void CourseManagement::refresh_all_problems_submit_and_accept_num_in_course(ojv4::c_id_type c_id)
{
	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	auto conn = conn_pool::fetch();

	mysqlpp::Query query = conn->query(
			"select u_id, p_id, s_result from solution "
			"where c_id = %0 and u_id in (select u_id from course_user where c_id = %1) order by s_id"
			// 注: 需要过滤掉不在课程中的用户 (考虑到有加人再删人的情形)
			// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
	);
	query.parse();

	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, id_type_hash<ojv4::p_id_type>> m;
	{
		mysqlpp::StoreQueryResult solutions = query.store(c_id, c_id);

		if(query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query problems' submit and accept num in course failed!", e, " c_id: ", c_id);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			m[p_id].add_solution(u_id, s_result);
		}
	}

	std::vector<std::pair<ojv4::p_id_type, std::future<mysqlpp::SimpleResult>>> future_group;

	for (const auto & ele : m) {
		ojv4::p_id_type p_id = ele.first;
		const ProblemSARecorder & p_info = ele.second;
		int submit = p_info.submit_num();
		int accept = p_info.accept_num();

		auto as = [](conn_pool::auto_revert_t conn, ojv4::c_id_type c_id, ojv4::p_id_type p_id, int submit, int accept) -> mysqlpp::SimpleResult
		{
			mysqlpp::Query update = conn->query(
					"update course_problem set c_p_submit = %2, c_p_accept = %3 "
					"where c_id = %0 and p_id = %1"
			);
			update.parse();
			mysqlpp::SimpleResult res = update.execute(c_id, p_id, submit, accept);
			if(!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				throw e;
			}
			return res;
		};
		conn_pool::auto_revert_t conn = conn_pool::block_fetch();
		future_group.emplace_back(p_id, std::async(std::launch::async, as, std::move(conn), c_id, p_id, submit, accept));
	}

	std::exception_ptr e_ptr;
	for(auto & ele : future_group) {
		ojv4::p_id_type p_id = ele.first;
		try {
			mysqlpp::SimpleResult res = ele.second.get();
		} catch(const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Refresh problem's submit and accept num in course failed!", e, " c_id: ", c_id, " p_id: ", p_id);
			if(e_ptr == nullptr) {
				// 保存首个异常
				e_ptr = std::current_exception();
			}
		}
	}
	if(e_ptr != nullptr) {
		std::rethrow_exception(e_ptr);
	}
}

void CourseManagement::update_user_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id)
{
	UserSARecorder u_info;
	{
		mysqlpp::Query query = conn.query(
				"select p_id, s_result from solution "
				"where c_id = %0 and p_id in (select p_id from course_problem where c_id = %1) and u_id = %2 order by s_id"
				// 注: 需要过滤掉不在课程中的题目 (考虑到有加题再删题的情形)
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::StoreQueryResult solutions = query.store(c_id, c_id, u_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query user's solutions failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			u_info.add_solution(p_id, s_result);
		}
	}

	{
		mysqlpp::Query update = conn.query(
				"update course_user set c_submit = %2, c_accept = %3 "
				"where c_id = %0 and u_id = %1"
		);
		update.parse();
		mysqlpp::SimpleResult res;
		try {
			res = update.execute(c_id, u_id, u_info.submit_num(), u_info.accept_num());
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num failed!", e, " c_id: ", c_id, " u_id: ", u_id);
			throw;
		}
		if (!res) {
			/*
			 * 注: 对于不在课程中的学生, update 不会出错, 不会执行到该分支
			 * 提示: 假设你在命令行中模拟执行该 update 语句, 会提示
			 * Query OK, 0 rows affected (0.01 sec)
			 * Rows matched: 0  Changed: 0  Warnings: 0
			 * where 子句不会匹配到相应的行, 也就不会出错了
			 */
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num failed!", e, " c_id: ", c_id, " u_id: ", u_id);
			throw e;
		}
	}
}

void CourseManagement::update_problem_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::p_id_type p_id)
{
	ProblemSARecorder p_info;
	{
		mysqlpp::Query query = conn.query(
				"select u_id, s_result from solution "
				"where c_id = %0 and u_id in (select u_id from course_problem where c_id = %1) and p_id = %2 order by s_id"
				// 注: 需要过滤掉不在课程中的用户 (考虑到有加人再删人的情形)
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::StoreQueryResult solutions = query.store(c_id, c_id, p_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query problem's solutions failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			p_info.add_solution(u_id, s_result);
		}
	}

	{
		mysqlpp::Query update = conn.query(
				"update course_problem set c_p_submit = %2, c_p_accept = %3 "
				"where c_id = %0 and p_id = %1"
		);
		update.parse();
		mysqlpp::SimpleResult res;
		try {
			res = update.execute(c_id, p_id, p_info.submit_num(), p_info.accept_num());
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num failed!", e, " c_id: ", c_id, " p_id: ", p_id);
			throw;
		}
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num failed!", e, " c_id: ", c_id, " p_id: ", p_id);
			throw e;
		}
	}
}

ojv4::u_p_status_enum query_course_user_s_problem_status_from_solution(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	constexpr ojv4::s_result_in_db_type ACCEPTED = ojv4::s_result_in_db_type(ojv4::s_result_enum::ACCEPTED);
	constexpr ojv4::s_result_in_db_type SYSTEM_ERROR = ojv4::s_result_in_db_type(ojv4::s_result_enum::SYSTEM_ERROR);

	mysqlpp::Query query_status = conn.query(
			"select ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and c_id = %4 and s_result <> %3)"
			") as has_submit, ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and c_id = %4 and s_result = %2)"
			") as has_accepted"
	);
	query_status.parse();

	mysqlpp::StoreQueryResult res = query_status.store(u_id, p_id, ACCEPTED, SYSTEM_ERROR, c_id);

	if(bool(res[0]["has_accepted"]) == true) {
		return ojv4::u_p_status_enum::ACCEPTED;
	} else if(bool(res[0]["has_submit"]) == true) {
		return ojv4::u_p_status_enum::ATTEMPTED;
	} else {
		return ojv4::u_p_status_enum::TODO;
	}
}

void CourseManagement::update_user_problem_status(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
//	ojv4::u_p_status_enum status = ojv4::u_p_status_enum::TODO;
//
//	try {
//		status = query_course_user_s_problem_status_from_solution(conn, c_id, u_id, p_id);
//	} catch (const std::exception & e) {
//		EXCEPT_FATAL(0, 0, log_fp, "Query course-user's problem status failed!", e);
//		throw;
//	}
//
//	if (status == ojv4::u_p_status_enum::TODO) {
//		return;
//	}
//
//	/*
//	 insert into course_user_problem_status(c_id, u_id, p_id, status)
//	 values(1, 1, 1001, 0)
//	 on duplicate key update status = 0
//	 */
//
//	mysqlpp::Query on_duplicate_insert = conn.query(
//			"insert into course_user_problem_status(u_id, p_id, status, c_id) "
//			"values(%0, %1, %2, %3) "
//			"on duplicate key update status = %2"
//	);
//	on_duplicate_insert.parse();
//
//	mysqlpp::SimpleResult res = on_duplicate_insert.execute(u_id, p_id, ojv4::u_p_status_type(status), c_id);
//
//	if(!res) {
//		MysqlEmptyResException e(on_duplicate_insert.errnum(), on_duplicate_insert.error());
//		EXCEPT_FATAL(0, 0, log_fp, "Update course-user's problem status failed!", e);
//		throw e;
//	}
}

void CourseManagement::update_user_problem(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	ojv4::u_p_status_enum status = ojv4::u_p_status_enum::TODO;

	try {
		status = query_course_user_s_problem_status_from_solution(conn, c_id, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Query course-user's problem status failed!", e);
		throw;
	}

	if (status == ojv4::u_p_status_enum::TODO) {
		return;
	}

	bool if_exists = false;
	{
		mysqlpp::Query query_if_exists = conn.query(
				"select id from user_problem "
				"where u_id = %0 and p_id = %1 and c_id = %2"
		);
		query_if_exists.parse();

		mysqlpp::StoreQueryResult query_if_exists_res = query_if_exists.store(u_id, p_id, c_id);
		if (query_if_exists.errnum() != 0) {
			MysqlEmptyResException e(query_if_exists.errnum(), query_if_exists.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query if exists user-problem failed!", e);
			throw e;
		}
		if (query_if_exists_res.empty()) {
			if_exists = false;
		} else {
			if_exists = true;
		}
	}


	if(if_exists == false) {

		mysqlpp::Query insert = conn.query(
				"insert into user_problem (u_id, p_id, status, c_id) "
				"values (%0, %1, %2, %3)"
		);
		insert.parse();

		mysqlpp::SimpleResult insert_res = insert.execute(u_id, p_id, ojv4::u_p_status_type(status), c_id);
		if (!insert_res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	} else {
		mysqlpp::Query update = conn.query(
				"update user_problem set "
				"status = %0 "
				"where u_id = %1 and p_id = %2 and c_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(ojv4::u_p_status_type(status), u_id, p_id, c_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	}
}


