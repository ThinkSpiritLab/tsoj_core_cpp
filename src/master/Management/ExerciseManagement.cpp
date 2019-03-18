/*
 * ExerciseManagement.cpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#include "ExerciseManagement.hpp"
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

void ExerciseManagement::refresh_all_users_sa_num(mysqlpp::Connection & mysql_conn)
{
	std::unordered_map<oj::u_id_type, UserSARecorder, oj::u_id_type::hash> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);

		// 性能优化提示 1: 这边用 Query::use(), 比使用 store() 方法先把数据保存下来然后再遍历效率要更高
		mysqlpp::UseQueryResult solutions = query.use();
		if (!solutions) {
			throw std::runtime_error("Query users' solutions in exercise failed!"s
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			/*
			 * 性能优化提示 2:
			 * 使用 C 风格版裸的 MYSQL_ROW 类型比 C++ 风格的 mysql::row 类型效率要高,
			 * 因为前者就是 char**，而后者是对 vector<string> 的封装
			 * 当然改用 MYSQL_ROW 的代价是少了类型检查、越界检查等, 代码不安全.
			 * 当然该函数工作量较大, 这里是为了性能考虑, 不得不做妥协
			 *
			 * 性能优化提示 3:
			 * 使用 boost::lexical_cast 而不使用 stox 的理由是:
			 * stox 的参数为 const std::string&, 会从 const char* 构造一个 string 类型临时变量, 效率低;
			 * 而 boost::lexical_cast 支持从 const char* 转换
			 * 不使用 C 语言风格的 atox 系列函数的理由是: atox 没有异常机制, 如若传进去的字符串非法就可能有隐患
			 * 尽管 boost::lexical_cast 的效率略低于 atox, 但在满足安全原则的前提下低不了多少
			 */
			auto u_id(boost::lexical_cast<oj::u_id_type>(row[0]));
			auto p_id(boost::lexical_cast<oj::p_id_type>(row[1]));
			auto s_result(oj::s_result_enum(boost::lexical_cast<int>(row[2])));
			m[u_id].add_solution(p_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);
	
	// 刷新前，先把所有用户的 SA 数置零
	if (!mysql_conn.query("update user set u_submit = 0, u_accept = 0").execute()) {
		throw std::runtime_error("Clear users' submit and accept num in exercise failed!"s
				+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
				+ " MySQL error: " + mysql_conn.error());
	}

	mysqlpp::Query update = mysql_conn.query(
			"update user set u_submit = %1, u_accept = %2 "
			"where u_id = %0"
	);
	update.parse();

	for (const auto & [u_id, u_sa_info] : m) {
		if (!update.execute(u_id, u_sa_info.submit_num(), u_sa_info.accept_num())) {
			throw std::runtime_error("Update user's submit and accept num in exercise failed!"s
					+ " u_id = " + std::to_string(u_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}
	trans.commit();
}

void ExerciseManagement::refresh_all_problems_sa_num(mysqlpp::Connection & mysql_conn)
{
	std::unordered_map<oj::p_id_type, ProblemSARecorder, oj::p_id_type::hash> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
		// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);

		mysqlpp::UseQueryResult solutions = query.use();
		if (!solutions) {
			throw std::runtime_error("Query problems' solutions in exercise failed!"s
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<oj::u_id_type>(row[0]));
			auto p_id(boost::lexical_cast<oj::p_id_type>(row[1]));
			auto s_result(oj::s_result_enum(boost::lexical_cast<int>(row[2])));
			m[p_id].add_solution(u_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	if (!mysql_conn.query("update problem set p_submit = 0, p_accept = 0").execute()) {
		throw std::runtime_error("Clear problems' submit and accept num in exercise failed!"s
				+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
				+ " MySQL error: " + mysql_conn.error());
	}

	mysqlpp::Query update = mysql_conn.query(
			"update problem set p_submit = %1, p_accept = %2 "
			"where p_id = %0"
	);
	update.parse();

	for (const auto & [p_id, p_sa_info] : m) {
		if (!update.execute(p_id, p_sa_info.submit_num(), p_sa_info.accept_num())) {
			throw std::runtime_error("Update problem's submit and accept num in exercise failed!"s
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}
	trans.commit();
}

void ExerciseManagement::refresh_all_user_problem(mysqlpp::Connection & mysql_conn)
{
	struct pair_hash
	{
			size_t operator()(const std::pair<oj::u_id_type, oj::p_id_type> & p) const
			{
//				return id_type_hash<oj::u_id_type>()(p.first) ^ id_type_hash<oj::p_id_type>()(p.second);

				size_t hash = oj::u_id_type::hash()(p.first);
				hash = hash * 48271UL + oj::p_id_type::hash()(p.second);
				return hash;
			}
	};

	struct tuple_hash
	{
			size_t operator()(const std::tuple<oj::u_id_type, oj::p_id_type, oj::c_id_type> & p) const
			{
//				return
//				 id_type_hash<oj::u_id_type>()(std::get<0>(p)) +
//				 id_type_hash<oj::p_id_type>()(std::get<1>(p)) +
//				 id_type_hash<oj::c_id_type>()(std::get<2>(p));

				/*
				 * 一个适用于 pair, tuple 的低冲突概率哈希策略的灵感来源:
				 * 由于 STL 没有为 pair 及 tuple 提供默认的哈希实现, 因此如果想使用 pair 或 tuple
				 * 做为 unordered_map 的键类型的话, 就需要我们自己实现一个哈希策略.
				 * 在初步编码时, 为了方便, 笔者选择了最简单的方案, 即: hash(tuple) = hash(tuple[0]) + hash(tuple[1]) + hash(tuple[2])
				 * 在优化此方法时, 笔者使用 MySQL 中的全真数据做了一下测试 (当时 max(s_id) = 214601),
				 * 发现部分哈希值碰撞次数达到 20 - 30 甚至 50 之多. 而我们知道, 为了提高效率, 哈希冲突显然是越小越好
				 * 于是第二版的哈希策略我借鉴了标准库 <random> 中 minstd_rand 的实现原理
				 *
				 * typedef linear_congruential_engine<uint_fast32_t, 16807UL, 0UL, 2147483647UL>
				 * minstd_rand0;
				 * typedef linear_congruential_engine<uint_fast32_t, 48271UL, 0UL, 2147483647UL>
				 * minstd_rand;
				 *
				 * minstd_rand 是一个代入了特化参数的线性同余随机数引擎, linear_congruential_engine 的数学本质就是,
				 * 在上一次产生的随机数的基础上, 乘以一个较大的素数, 再加上一个偏移量, 最后对所设定的最大上限求余, 得到下一个随机数.
				 * 这样, 就以较为简单的方式, 产生了一系列分布较为均匀的伪随机数. 而这里的 "分布均匀" 不就是我们所追求的 "低冲突" 吗?
				 * 最后, 笔者用优化过的哈希策略使用同样的数据进行测试, 发现竟一次冲突也没有发生!
				 * 同时, 两个 map 的计算时间也由 700 ms 降低到了 600 ms
				 */
				size_t hash = oj::u_id_type::hash()(std::get<0>(p));
				hash = hash * 48271UL + oj::p_id_type::hash()(std::get<1>(p));
				hash = hash * 48271UL + oj::c_id_type::hash()(std::get<2>(p));
				return hash;
			}
	};

	std::unordered_map<std::pair<oj::u_id_type, oj::p_id_type>, oj::u_p_status_enum, pair_hash> u_p_status;
	std::unordered_map<std::tuple<oj::u_id_type, oj::p_id_type, oj::c_id_type>, oj::u_p_status_enum, tuple_hash> u_p_status_of_course;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, ( "
				"	case "
				"	when c_id is NULL then NULL " // 原本就是练习中的提交, 不做任何处理
				"	when ( u_id in (select u_id from course_user where c_id = solution.c_id) and "
				"		   p_id in (select p_id from course_problem where c_id = solution.c_id) "
				"	) then c_id " // 人/题还在课程中, 则使用原 c_id
				"	else NULL " // 如果是被删过的人/题, 则仍被看作是练习中的一次提交
				"	end "
				") as c_id_considering_delete, s_result "
				"from solution order by s_id"
		);
		/*
		 * c_id_considering_delete 的含义:
		 * 基于之前的文档我们知道, 如果课程里删除过人或者题以后, 那么计算提交数通过数时需要过滤掉已删除的人/题;
		 * 同样的, 在重算 user-problem 时也需要考虑到这个问题.
		 * 我们知道, 在课程中提交了一次会被视为也在练习中提交了一次处理的.
		 * 所以, 如果课程中删人了, 那我们在此约定, 受到影响的 solution 不再算作是课程中的提交, 但是仍算作是练习中的一次提交
		 */
		mysqlpp::UseQueryResult solutions = query.use();

		if (!solutions) {
			throw std::runtime_error("Query solutions failed!"s
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<oj::u_id_type>(row[0]));
			auto p_id(boost::lexical_cast<oj::p_id_type>(row[1]));
			auto s_result(oj::s_result_enum(boost::lexical_cast<int>(row[3])));
			switch (s_result) {
				case oj::s_result_enum::ACCEPTED:
					u_p_status[ { u_id, p_id }] = oj::u_p_status_enum::ACCEPTED;
					if (row[2] != nullptr) {
						auto c_id(boost::lexical_cast<oj::c_id_type>(row[2]));
						u_p_status_of_course[ { u_id, p_id, c_id }] = oj::u_p_status_enum::ACCEPTED;
					}
					break;
				case oj::s_result_enum::SYSTEM_ERROR:
					break;
				default:
					//inserts if the key does not exist, does nothing if the key exists
					u_p_status.insert( { { u_id, p_id }, oj::u_p_status_enum::ATTEMPTED });
					if (row[2] != nullptr) {
						auto c_id(boost::lexical_cast<oj::c_id_type>(row[2]));
						u_p_status_of_course.insert( { { u_id, p_id, c_id }, oj::u_p_status_enum::ATTEMPTED });
					}
			}
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	if (!(mysql_conn.query("delete from user_problem").execute())) {
		throw std::runtime_error("Clear user_problem table failed!"s
				+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
				+ " MySQL error: " + mysql_conn.error());
	}

	{
		mysqlpp::Query insert = mysql_conn.query(
				"insert into user_problem(u_id, p_id, status, c_id) "
				"values("
		);
		auto begin = u_p_status.cbegin();
		auto end = u_p_status.cend();

		if (begin != end) {
			const auto & [key, status] = *begin;
			const auto & [u_id, p_id] = key;
			insert << u_id << ',' << p_id << ',' << int(status) << ",null)";

			while (++begin != end) {
				const auto & [key, status] = *begin;
				const auto & [u_id, p_id] = key;
				insert << ",(" << u_id << ',' << p_id << ',' << int(status) << ",null)";
			}
			if (!insert.execute()) {
				throw std::runtime_error("Update user_problem table failed!"s
						+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
						+ " MySQL error: " + mysql_conn.error());
			}
		}
	}

	{
		mysqlpp::Query insert = mysql_conn.query(
				"insert into user_problem(u_id, p_id, status, c_id) "
				"values("
		);
		auto begin = u_p_status_of_course.cbegin();
		auto end = u_p_status_of_course.cend();

		if (begin != end) {
			const auto & [key, status] = *begin;
			const auto & [u_id, p_id, c_id] = key;
			insert << u_id << ',' << p_id << ',' << int(status) << ',' << c_id << ')';

			while (++begin != end) {
				const auto & [key, status] = *begin;
				const auto & [u_id, p_id, c_id] = key;
				insert << ",(" << u_id << ',' << p_id << ',' << int(status) << ',' << c_id << ')';
			}
			if (!insert.execute()) {
				throw std::runtime_error("Update user_problem table failed!"s
						+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
						+ " MySQL error: " + mysql_conn.error());
			}
		}
	}

	trans.commit();
}


void ExerciseManagement::update_user_s_sa_num(mysqlpp::Connection & conn, oj::u_id_type u_id)
{
	UserSARecorder u_info;
	{
		mysqlpp::Query query = conn.query(
				"select p_id, s_result from solution "
				"where u_id = %0 order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(u_id);
		if (!solutions) {
			throw std::runtime_error("Query user's solutions failed!"s
					+ " u_id = " + std::to_string(u_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto p_id(boost::lexical_cast<oj::p_id_type>(row[0]));
			auto s_result(oj::s_result_enum(boost::lexical_cast<int>(row[1])));
			u_info.add_solution(p_id, s_result);
		}
	}

	mysqlpp::Query update = conn.query(
			"update user set u_submit = %1, u_accept = %2 "
			"where u_id = %0"
	);
	update.parse();
	if (!update.execute(u_id, u_info.submit_num(), u_info.accept_num())) {
		throw std::runtime_error("Update user's submit and accept num failed!"s
				+ " u_id = " + std::to_string(u_id)
				+ " MySQL errnum: " + std::to_string(conn.errnum())
				+ " MySQL error: " + conn.error());
	}
}

void ExerciseManagement::update_problem_s_sa_num(mysqlpp::Connection & conn, oj::p_id_type p_id)
{
	ProblemSARecorder p_info;
	{
		mysqlpp::Query query = conn.query(
				"select u_id, s_result from solution "
				"where p_id = %0 order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(p_id);
		if (!solutions) {
			throw std::runtime_error("Query problem's solutions failed!"s
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<oj::u_id_type>(row[0]));
			auto s_result(oj::s_result_enum(boost::lexical_cast<int>(row[1])));
			p_info.add_solution(u_id, s_result);
		}
	}

	mysqlpp::Query update = conn.query(
			"update problem set p_submit = %1, p_accept = %2 "
			"where p_id = %0"
	);
	update.parse();
	if (!update.execute(p_id, p_info.submit_num(), p_info.accept_num())) {
		throw std::runtime_error("Update problem's submit and accept num failed!"s
				+ " p_id = " + std::to_string(p_id)
				+ " MySQL errnum: " + std::to_string(conn.errnum())
				+ " MySQL error: " + conn.error());
	}
}

oj::u_p_status_enum query_user_s_problem_status_from_solution(mysqlpp::Connection & conn, oj::u_id_type u_id, oj::p_id_type p_id)
{
	constexpr oj::s_result_in_db_type ACCEPTED = oj::s_result_in_db_type(oj::s_result_enum::ACCEPTED);
	constexpr oj::s_result_in_db_type SYSTEM_ERROR = oj::s_result_in_db_type(oj::s_result_enum::SYSTEM_ERROR);

	mysqlpp::Query query_status = conn.query(
			"select ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and s_result <> %3)"
			") as has_submit, ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and s_result = %2)"
			") as has_accepted"
	); // 不要加 c_id is null 的判断！！！ 用户在课程中提交一题, 在练习中看到的状态也是 AC 的
	query_status.parse();

	mysqlpp::StoreQueryResult res = query_status.store(u_id, p_id, ACCEPTED, SYSTEM_ERROR);

	if (bool(res[0]["has_accepted"]) == true) {
		return oj::u_p_status_enum::ACCEPTED;
	} else if (bool(res[0]["has_submit"]) == true) {
		return oj::u_p_status_enum::ATTEMPTED;
	} else {
		return oj::u_p_status_enum::TODO;
	}
}

void ExerciseManagement::update_user_problem(mysqlpp::Connection & conn, oj::u_id_type u_id, oj::p_id_type p_id)
{
	oj::u_p_status_enum new_status = oj::u_p_status_enum::ATTEMPTED;

	try {
		new_status = query_user_s_problem_status_from_solution(conn, u_id, p_id);
	} catch (const std::exception & e) {
		throw std::runtime_error("Query user's problem status failed!"s
				+ " u_id = " + std::to_string(u_id)
				+ " p_id = " + std::to_string(p_id)
				+ " details: " + e.what());
	}

	if (new_status == oj::u_p_status_enum::TODO) {
		return;
	}

	oj::u_p_status_enum previous_status = oj::u_p_status_enum::TODO;
	{
		mysqlpp::Query query = conn.query(
				"select status from user_problem "
				"where u_id = %0 and p_id = %1 and c_id is null");
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(u_id, p_id);
		if (!res) {
			throw std::runtime_error("Query previous status from user-problem failed!"s
					+ " u_id = " + std::to_string(u_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}
		if (res.empty()) {
			previous_status = oj::u_p_status_enum::TODO;
		} else {
			previous_status = oj::u_p_status_enum(int(res[0]["status"]));
		}
	}

	if (previous_status == oj::u_p_status_enum::TODO) {
		mysqlpp::Query insert = conn.query(
				"insert into user_problem (u_id, p_id, status, c_id) "
				"values (%0, %1, %2, null)"
		);
		insert.parse();
		if (!insert.execute(u_id, p_id, int(new_status))) {
			throw std::runtime_error("Insert user-problem failed!"s
					+ " u_id = " + std::to_string(u_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}
	} else if (previous_status != new_status) {
		mysqlpp::Query update = conn.query(
				"update user_problem set "
				"status = %0 "
				"where u_id = %1 and p_id = %2 and c_id is null"
		);
		update.parse();
		if (!update.execute(int(new_status), u_id, p_id)) {
			throw std::runtime_error("Update user-problem failed!"s
					+ " u_id = " + std::to_string(u_id)
					+ " p_id = " + std::to_string(p_id)
					+ " MySQL errnum: " + std::to_string(conn.errnum())
					+ " MySQL error: " + conn.error());
		}
	}

}


