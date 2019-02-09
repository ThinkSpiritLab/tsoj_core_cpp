/*
 * CourseManagement.cpp
 *
 *  Created on: 2018年11月11日
 *      Author: peter
 */

#include "CourseManagement.hpp"
#include "SARecorder.hpp"

#include <string>
#include <unordered_map>
#include <boost/lexical_cast.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>
#include <mysql++/connection.h>

using namespace std::string_literals;

void CourseManagement::refresh_all_users_submit_and_accept_num_in_course(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id)
{
	// 以下课程/考试的成绩较为敏感, 其中用户的提交数通过数不应当再刷新. 故使用硬编码的方式予以保护
	constexpr static ojv4::c_id_type protected_course[] = {
		16_c_id, // 2017级程序设计实验/实训
		17_c_id, // 2018研究生复试
		22_c_id, // 2017级程序设计实验/实训期末考试
		23_c_id, // 2017级程序设计实验/实训补考
		25_c_id, // 2018级程序设计基础
		26_c_id, // 2018秋季藕舫学院长望程序设计竞赛班
		27_c_id, // 2018-2019(1)程序设计实验补考
		28_c_id, // 2018年秋季藕坊学院程序设计竞赛期末考试
	};

	if (std::find(std::begin(protected_course), std::end(protected_course), c_id) != std::end(protected_course)) {
		return;
	}

	std::unordered_map<ojv4::u_id_type, UserSARecorder, ojv4::u_id_type::hash> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"where c_id = %0 and p_id in (select p_id from course_problem where c_id = solution.c_id) "
				// 注: 需要过滤掉不在课程中的题目 (考虑到有加题再删题的情形, 已被删除的题还算在学生的提交数通过数里, 显然是不合适的)
				// 尽管我们不建议在课程中删题, 但是出于防御性编程的目的考虑, 我们在这里还是要对这种出现的情形做处理
				// 思考: 需不需要过滤掉已不在课程中的学生呢? 答案在下面若干行
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(c_id);
		if (!solutions) {
			throw std::runtime_error("Query users' solutions in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(row[0]));
			auto p_id(boost::lexical_cast<ojv4::p_id_type>(row[1]));
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(row[2])));
			m[u_id].add_solution(p_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query clear = mysql_conn.query(
				"update course_user set c_submit = 0, c_accept = 0 "
				"where c_id = "
		);
		clear << c_id;
		if (!clear.execute()) {
			throw std::runtime_error("Clear users' submit and accept num in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	/*
	 * 针对上面思考题的答案: 不需要过滤掉不在课程中的学生
	 * 对于不在课程中的学生, update 语句的 where 子句 'where c_id = %0 and u_id = %1‘
	 * 的断言结果为假, 也就是说, 该 update 语句的 affect rows 为 0
	 * 提示: 假设你在命令行中模拟执行该 update 语句, 会提示:
	 * Query OK, 0 rows affected (0.01 sec)
	 * Rows matched: 0  Changed: 0  Warnings: 0
	 * where 子句不会匹配到相应的行, 也就说, 不需要在 select 时多加一层过滤
	 */
	mysqlpp::Query update = mysql_conn.query(
			"update course_user set c_submit = %2, c_accept = %3 "
			"where c_id = %0 and u_id = %1"
	);
	update.parse();

	for (const auto & [u_id, u_sa_info] : m) {
		if (!update.execute(c_id, u_id, u_sa_info.submit_num(), u_sa_info.accept_num())) {
			throw std::runtime_error("Refresh user's submit and accept num in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " u_id = " + std::to_string(u_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	trans.commit();
}

void CourseManagement::refresh_all_problems_submit_and_accept_num_in_course(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id)
{
	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, ojv4::p_id_type::hash> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"where c_id = %0 and u_id in (select u_id from course_user where c_id = solution.c_id) "
				// 注: 需要过滤掉不在课程中的用户 (考虑到有加人再删人的情形)
				"order by s_id"
				// order by s_id 是重中之重
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(c_id);
		if (!solutions) {
			throw std::runtime_error("Query problems' solutions in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(row[0]));
			auto p_id(boost::lexical_cast<ojv4::p_id_type>(row[1]));
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(row[2])));
			m[p_id].add_solution(u_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query clear = mysql_conn.query(
				"update course_problem set c_p_submit = 0, c_p_accept = 0 "
				"where c_id = "
		);
		clear << c_id;
		if (!clear.execute()) {
			throw std::runtime_error("Clear problems' submit and accept num in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update course_problem set c_p_submit = %2, c_p_accept = %3 "
			"where c_id = %0 and p_id = %1"
	);
	update.parse();

	for (const auto & [p_id, p_sa_info] : m) {
		if (!update.execute(c_id, p_id, p_sa_info.submit_num(), p_sa_info.accept_num())) {
			throw std::runtime_error("Refresh problem's submit and accept num in course failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	trans.commit();
}

void CourseManagement::update_user_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id)
{
	UserSARecorder u_info;
	{
		mysqlpp::Query query = conn.query(
				"select p_id, s_result from solution "
				"where c_id = %0 and u_id = %1 and "
				"p_id in (select p_id from course_problem where c_id = solution.c_id) "
				// 注: 需要过滤掉不在课程中的题目 (考虑到有加题再删题的情形)
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(c_id, u_id);

		if (!solutions) {
			throw std::runtime_error("Query user's solutions failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " u_id = " + std::to_string(u_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto p_id(boost::lexical_cast<ojv4::p_id_type>(row[0]));
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(row[1])));
			u_info.add_solution(p_id, s_result);
		}
	}

	mysqlpp::Query update = conn.query(
			"update course_user set c_submit = %2, c_accept = %3 "
			"where c_id = %0 and u_id = %1"
	);
	update.parse();
	if (!update.execute(c_id, u_id, u_info.submit_num(), u_info.accept_num())) {
		/*
		 * 注: 对于不在课程中的学生, update 不会出错, 不会执行到该分支
		 * 提示: 假设你在命令行中模拟执行该 update 语句, 会提示
		 * Query OK, 0 rows affected (0.01 sec)
		 * Rows matched: 0  Changed: 0  Warnings: 0
		 * where 子句不会匹配到相应的行, 也就不会出错了
		 */
		throw std::runtime_error("Update user's submit and accept num failed!"s
				+ " c_id = " + std::to_string(c_id)
				+ " u_id = " + std::to_string(u_id)
				+ " MySQL errnum: " + std::to_string(conn.errnum())
				+ " MySQL error: " + conn.error());
	}
}

void CourseManagement::update_problem_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::p_id_type p_id)
{
	ProblemSARecorder p_info;
	{
		mysqlpp::Query query = conn.query(
				"select u_id, s_result from solution "
				"where c_id = %0 and p_id = %1 and u_id in (select u_id from course_problem where c_id = solution.c_id) "
				// 注: 需要过滤掉不在课程中的用户 (考虑到有加人再删人的情形)
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(c_id, p_id);

		if (!solutions) {
			throw std::runtime_error("Query problem's solutions failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(row[0]));
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(row[1])));
			p_info.add_solution(u_id, s_result);
		}
	}

	mysqlpp::Query update = conn.query(
			"update course_problem set c_p_submit = %2, c_p_accept = %3 "
			"where c_id = %0 and p_id = %1"
	);
	update.parse();
	if (!update.execute(c_id, p_id, p_info.submit_num(), p_info.accept_num())) {
		throw std::runtime_error("Update problem's submit and accept num failed!"s
				+ " c_id = " + std::to_string(c_id)
				+ " p_id = " + std::to_string(p_id)
				+ " MySQL errnum: " + std::to_string(conn.errnum())
				+ " MySQL error: " + conn.error());
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

	if (bool(res[0]["has_accepted"]) == true) {
		return ojv4::u_p_status_enum::ACCEPTED;
	} else if (bool(res[0]["has_submit"]) == true) {
		return ojv4::u_p_status_enum::ATTEMPTED;
	} else {
		return ojv4::u_p_status_enum::TODO;
	}
}

void CourseManagement::update_user_problem_status(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
}

void CourseManagement::update_user_problem(mysqlpp::Connection & conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	ojv4::u_p_status_enum new_status = ojv4::u_p_status_enum::TODO;

	try {
		new_status = query_course_user_s_problem_status_from_solution(conn, c_id, u_id, p_id);
	} catch (const std::exception & e) {
		throw std::runtime_error("Query course-user's problem status failed!"s
				+ " c_id = " + std::to_string(c_id)
				+ " u_id = " + std::to_string(u_id)
				+ " p_id = " + std::to_string(p_id)
				+ " details: " + e.what());
	}

	if (new_status == ojv4::u_p_status_enum::TODO) {
		return;
	}

	ojv4::u_p_status_enum previous_status = ojv4::u_p_status_enum::TODO;
	{
		mysqlpp::Query query = conn.query(
				"select status from user_problem "
				"where u_id = %0 and p_id = %1 and c_id = %2");
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(u_id, p_id, c_id);
		if (!res) {
			throw std::runtime_error("Query previous status from user-problem failed!"s
					+ " c_id = " + std::to_string(c_id)
					+ " u_id = " + std::to_string(u_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}
		if (res.empty()) {
			previous_status = ojv4::u_p_status_enum::TODO;
		} else {
			previous_status = ojv4::u_p_status_enum(int(res[0]["status"]));
		}
	}

	if (previous_status == ojv4::u_p_status_enum::TODO) {
		mysqlpp::Query insert = conn.query(
				"insert into user_problem (u_id, p_id, status, c_id) "
				"values (%0, %1, %2, %3)"
		);
		insert.parse();
        if (!insert.execute(u_id, p_id, int(new_status), c_id)) {
            throw std::runtime_error("Insert user-problem failed!"s
             		+ " c_id = " + std::to_string(c_id)
                    + " u_id = " + std::to_string(u_id)
                    + " p_id = " + std::to_string(p_id)
                    + " MySQL errnum: " + std::to_string(conn.errnum())
                    + " MySQL error: " + conn.error());
        }
	} else if (previous_status != new_status) {
		mysqlpp::Query update = conn.query(
				"update user_problem set "
				"status = %0 "
				"where u_id = %1 and p_id = %2 and c_id = %3"
		);
		update.parse();
        if (!update.execute(int(new_status), u_id, p_id, c_id)) {
            throw std::runtime_error("Update user-problem failed!"s
             		+ " c_id = " + std::to_string(c_id)
             		+ " u_id = " + std::to_string(u_id)
             		+ " p_id = " + std::to_string(p_id)
             		+ " MySQL errnum: " + std::to_string(conn.errnum())
             		+ " MySQL error: " + conn.error());
        }
    }
}


