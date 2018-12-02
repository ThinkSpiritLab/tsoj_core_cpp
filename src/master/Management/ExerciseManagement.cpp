/*
 * ExerciseManagement.cpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#include "ExerciseManagement.hpp"
#include "mysql_empty_res_exception.hpp"
#include "logger.hpp"
#include "SARecorder.hpp"

#include <unordered_map>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

extern std::ofstream log_fp;


void ExerciseManagement::refresh_all_users_submit_and_accept_num(mysqlpp::Connection & mysql_conn)
{
	kerbal::data_struct::optional<int> finished;
	kerbal::data_struct::optional<int> total;
	ExerciseManagement::refresh_all_users_submit_and_accept_num(mysql_conn, finished, total);
}


void ExerciseManagement::refresh_all_users_submit_and_accept_num(mysqlpp::Connection & mysql_conn,
																	kerbal::data_struct::optional<int> & finished,
																	kerbal::data_struct::optional<int> & total)
try {
	std::unordered_map<ojv4::u_id_type, UserSARecorder, id_type_hash<ojv4::u_id_type>> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		mysqlpp::StoreQueryResult solutions = query.store();

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query users' submit and accept num failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			m[u_id].add_solution(p_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query clear = mysql_conn.query(
				"update user set u_submit = 0, u_accept = 0"
		);
		mysqlpp::SimpleResult res = clear.execute();
		if (!res) {
			MysqlEmptyResException e(clear.errnum(), clear.error());
			EXCEPT_FATAL(0, 0, log_fp, "Clear users' submit and accept num in exercise failed!", e);
			throw e;
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update user set u_submit = %1, u_accept = %2 "
			"where u_id = %0"
	);
	update.parse();

	total = m.size();
	finished = 0;
	for (const auto & ele : m) {
		ojv4::u_id_type u_id = ele.first;
		const UserSARecorder & u_info = ele.second;
		int submit = u_info.submit_num();
		int accept = u_info.accept_num();

		mysqlpp::SimpleResult res = update.execute(u_id, submit, accept);
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num in exercise failed!", e, " u_id: ", u_id);
			throw e;
		}
		++*finished;
	}

	trans.commit();
} catch (...) {
	total = 0;
	finished = 0;
	throw;
}


void ExerciseManagement::refresh_all_problems_submit_and_accept_num(mysqlpp::Connection & mysql_conn)
{
	kerbal::data_struct::optional<int> finished;
	kerbal::data_struct::optional<int> total;
	ExerciseManagement::refresh_all_problems_submit_and_accept_num(mysql_conn, finished, total);
}

void ExerciseManagement::refresh_all_problems_submit_and_accept_num(mysqlpp::Connection & mysql_conn,
																			kerbal::data_struct::optional<int> & finished,
																			kerbal::data_struct::optional<int> & total)
try {
	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, id_type_hash<ojv4::p_id_type>> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
		// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		mysqlpp::StoreQueryResult solutions = query.store();

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query problems' submit and accept num failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			m[p_id].add_solution(u_id, s_result);
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query clear = mysql_conn.query(
				"update problem set p_submit = 0, p_accept = 0"
		);
		mysqlpp::SimpleResult res = clear.execute();
		if (!res) {
			MysqlEmptyResException e(clear.errnum(), clear.error());
			EXCEPT_FATAL(0, 0, log_fp, "Clear problems' submit and accept num in exercise failed!", e);
			throw e;
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update problem set p_submit = %1, p_accept = %2 "
			"where p_id = %0"
	);
	update.parse();

	total = m.size();
	finished = 0;
	for (const auto & ele : m) {
		ojv4::p_id_type p_id = ele.first;
		const ProblemSARecorder & p_info = ele.second;
		int submit = p_info.submit_num();
		int accept = p_info.accept_num();

		mysqlpp::SimpleResult res = update.execute(p_id, submit, accept);
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num in exercise failed!", e, " p_id: ", p_id);
			throw e;
		}
		++*finished;
	}
	trans.commit();
} catch (...) {
	total = 0;
	finished = 0;
	throw;
}

void ExerciseManagement::refresh_all_user_problem(mysqlpp::Connection & mysql_conn)
{
	struct pair_hash
	{
			size_t operator()(const std::pair<ojv4::u_id_type, ojv4::p_id_type> & p) const
			{
				return id_type_hash<ojv4::u_id_type>()(p.first) ^ id_type_hash<ojv4::p_id_type>()(p.second);
			}
	};

	std::unordered_map<std::pair<ojv4::u_id_type, ojv4::p_id_type>, ojv4::u_p_status_enum, pair_hash> u_p_status;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, s_result from solution order by s_id"
		);
		mysqlpp::StoreQueryResult solutions = query.store();

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query solutions failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			switch (s_result) {
				case ojv4::s_result_enum::ACCEPTED:
					u_p_status[ { u_id, p_id }] = ojv4::u_p_status_enum::ACCEPTED;
					break;
				case ojv4::s_result_enum::SYSTEM_ERROR:
					break;
				default:
					//inserts if the key does not exist, does nothing if the key exists
					u_p_status.insert( { { u_id, p_id }, ojv4::u_p_status_enum::ATTEMPTED });
			}
		}
	}

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query delete_table = mysql_conn.query(
				"delete from user_problem"
		);
		mysqlpp::SimpleResult res = delete_table.execute();
		if(!res) {
			MysqlEmptyResException e(delete_table.errnum(), delete_table.error());
			EXCEPT_FATAL(0, 0, log_fp, "Clear user_problem table failed!", e);
			throw e;
		}
	}

	{
		mysqlpp::Query insert = mysql_conn.query(
				"insert into user_problem(u_id, p_id, status, c_id) values"
		);

		for (auto it = u_p_status.cbegin(); it != u_p_status.cend(); ++it) {
			ojv4::u_id_type u_id = it->first.first;
			ojv4::p_id_type p_id = it->first.second;
			ojv4::u_p_status_enum status = it->second;
			insert << "(" << u_id << ',' << p_id << ',' << int(ojv4::u_p_status_type(status)) << ",null)";

			if (std::next(it) != u_p_status.cend()) {
				insert << ',';
			}
		}

		mysqlpp::SimpleResult res = insert.execute();
		if (!res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Insert failed!", e);
			throw e;
		}
	}

	trans.commit();
}

void ExerciseManagement::refresh_all_user_problem2(mysqlpp::Connection & mysql_conn)
{
	struct pair_hash
	{
			size_t operator()(const std::pair<ojv4::u_id_type, ojv4::p_id_type> & p) const
			{
				return id_type_hash<ojv4::u_id_type>()(p.first) ^ id_type_hash<ojv4::p_id_type>()(p.second);
			}
	};

	struct tuple_hash
	{
			size_t operator()(const std::tuple<ojv4::u_id_type, ojv4::p_id_type, ojv4::c_id_type> & p) const
			{
				return id_type_hash<ojv4::u_id_type>()(std::get<0>(p)) + id_type_hash<ojv4::p_id_type>()(std::get<1>(p)) + id_type_hash<ojv4::c_id_type>()(std::get<2>(p));
			}
	};

	std::unordered_map<std::pair<ojv4::u_id_type, ojv4::p_id_type>, ojv4::u_p_status_enum, pair_hash> u_p_status;
	std::unordered_map<std::tuple<ojv4::u_id_type, ojv4::p_id_type, ojv4::c_id_type>, ojv4::u_p_status_enum, tuple_hash> u_p_status_of_course;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_id, c_id, s_result from solution order by s_id"
		);
		mysqlpp::StoreQueryResult solutions = query.store();

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query solutions failed!", e);
			throw e;
		}

		for (const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			switch (s_result) {
				case ojv4::s_result_enum::ACCEPTED:
					u_p_status[ { u_id, p_id }] = ojv4::u_p_status_enum::ACCEPTED;
					break;
				case ojv4::s_result_enum::SYSTEM_ERROR:
					break;
				default:
					//inserts if the key does not exist, does nothing if the key exists
					u_p_status.insert( { { u_id, p_id }, ojv4::u_p_status_enum::ATTEMPTED });
			}
			if (row["c_id"] != mysqlpp::null) {
				ojv4::c_id_type c_id = row["c_id"];
				switch (s_result) {
					case ojv4::s_result_enum::ACCEPTED:
						u_p_status_of_course[ { u_id, p_id, c_id }] = ojv4::u_p_status_enum::ACCEPTED;
						break;
					case ojv4::s_result_enum::SYSTEM_ERROR:
						break;
					default:
						//inserts if the key does not exist, does nothing if the key exists
						u_p_status_of_course.insert( { { u_id, p_id, c_id }, ojv4::u_p_status_enum::ACCEPTED });
				}
			}
		}
	}

	LOG_INFO(0, 0, log_fp, "count: ", u_p_status.size(), "  , count: ", u_p_status_of_course.size());

	mysqlpp::Transaction trans(mysql_conn);

	{
		mysqlpp::Query delete_table = mysql_conn.query(
				"delete from user_problem"
		);
		mysqlpp::SimpleResult res = delete_table.execute();
		if (!res) {
			MysqlEmptyResException e(delete_table.errnum(), delete_table.error());
			EXCEPT_FATAL(0, 0, log_fp, "Clear user_problem table failed!", e);
			throw e;
		}
	}

	{
		mysqlpp::Query insert = mysql_conn.query(
				"insert into user_problem(u_id, p_id, status, c_id) "
				"values(%0, %1, %2, %3)"
		);
		insert.parse();

		for (auto it = u_p_status.cbegin(); it != u_p_status.cend(); ++it) {
			ojv4::u_id_type u_id = it->first.first;
			ojv4::p_id_type p_id = it->first.second;
			ojv4::u_p_status_enum status = it->second;
			mysqlpp::SimpleResult res = insert.execute(u_id, p_id, int(ojv4::u_p_status_type(status)), mysqlpp::null);
			if (!res) {
				MysqlEmptyResException e(insert.errnum(), insert.error());
				EXCEPT_FATAL(0, 0, log_fp, "Insert failed!", e);
				throw e;
			}
		}
	}

	{
		mysqlpp::Query insert = mysql_conn.query(
				"insert into user_problem(u_id, p_id, status, c_id) "
				"values(%0, %1, %2, %3)"
		);
		insert.parse();

		for (auto it = u_p_status_of_course.cbegin(); it != u_p_status_of_course.cend(); ++it) {
			ojv4::u_id_type u_id = std::get<0>(it->first);
			ojv4::p_id_type p_id = std::get<1>(it->first);
			ojv4::c_id_type c_id = std::get<2>(it->first);
			ojv4::u_p_status_enum status = it->second;
			mysqlpp::SimpleResult res = insert.execute(u_id, p_id, int(ojv4::u_p_status_type(status)), c_id);
			if (!res) {
				MysqlEmptyResException e(insert.errnum(), insert.error());
				EXCEPT_FATAL(0, 0, log_fp, "Insert failed!", e);
				throw e;
			}
		}
	}

	trans.commit();
}


void ExerciseManagement::update_user_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::u_id_type u_id)
{
	UserSARecorder u_info;
	{
		mysqlpp::Query query = conn.query(
				"select p_id, s_result from solution "
				"where u_id = %0 order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::StoreQueryResult solutions = query.store(u_id);

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

	mysqlpp::Query update = conn.query(
			"update user set u_submit = %1, u_accept = %2 "
			"where u_id = %0"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(u_id, u_info.submit_num(), u_info.accept_num());
	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num failed!", e, " u_id: ", u_id);
		throw e;
	}
}

void ExerciseManagement::update_problem_s_submit_and_accept_num(mysqlpp::Connection & conn, ojv4::p_id_type p_id)
{
	ProblemSARecorder p_info;
	{
		mysqlpp::Query query = conn.query(
				"select u_id, s_result from solution "
				"where p_id = %0 order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::StoreQueryResult solutions = query.store(p_id);

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

	mysqlpp::Query update = conn.query(
			"update problem set p_submit = %1, p_accept = %2 "
			"where p_id = %0"
	);
	update.parse();
	mysqlpp::SimpleResult res = update.execute(p_id, p_info.submit_num(), p_info.accept_num());
	if (!res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num failed!", e, " p_id: ", p_id);
		throw e;
	}
}

ojv4::u_p_status_enum query_user_s_problem_status_from_solution(mysqlpp::Connection & conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	constexpr ojv4::s_result_in_db_type ACCEPTED = ojv4::s_result_in_db_type(ojv4::s_result_enum::ACCEPTED);
	constexpr ojv4::s_result_in_db_type SYSTEM_ERROR = ojv4::s_result_in_db_type(ojv4::s_result_enum::SYSTEM_ERROR);

	mysqlpp::Query query_status = conn.query(
			"select ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and s_result <> %3)"
			") as has_submit, ("
			"	select exists(select s_id from solution where u_id = %0 and p_id = %1 and s_result = %2)"
			") as has_accepted"
	); // 不要加 c_id is null 的判断！！！ 用户在课程中提交一题, 在练习中看到的状态也是 AC 的
	query_status.parse();

	mysqlpp::StoreQueryResult res = query_status.store(u_id, p_id, ACCEPTED, SYSTEM_ERROR);

	if(bool(res[0]["has_accepted"]) == true) {
		return ojv4::u_p_status_enum::ACCEPTED;
	} else if(bool(res[0]["has_submit"]) == true) {
		return ojv4::u_p_status_enum::ATTEMPTED;
	} else {
		return ojv4::u_p_status_enum::TODO;
	}
}

void ExerciseManagement::update_user_problem_status(mysqlpp::Connection & conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
//	ojv4::u_p_status_enum status = ojv4::u_p_status_enum::TODO;
//
//	try {
//		status = query_user_s_problem_status_from_solution(conn, u_id, p_id);
//	} catch (const std::exception & e) {
//		EXCEPT_FATAL(0, 0, log_fp, "Query user's problem status from solution failed!", e);
//		throw;
//	}
//
//	if (status == ojv4::u_p_status_enum::TODO) {
//		return;
//	}
//
//
////	 insert into user_problem_status(u_id, p_id, status)
////	 values(1, 1001, 0)
////	 on duplicate key update status = 0
//
//	mysqlpp::Query on_duplicate_insert = conn.query(
//			"insert into user_problem_status(u_id, p_id, status) "
//			"values(%0, %1, %2) "
//			"on duplicate key update status = %2"
//	);
//	on_duplicate_insert.parse();
//
//	mysqlpp::SimpleResult res = on_duplicate_insert.execute(u_id, p_id, ojv4::u_p_status_type(status));
//
//	if (!res) {
//		MysqlEmptyResException e(on_duplicate_insert.errnum(), on_duplicate_insert.error());
//		EXCEPT_FATAL(0, 0, log_fp, "Update user's problem status failed!", e);
//		throw e;
//	}
}

void ExerciseManagement::update_user_problem(mysqlpp::Connection & conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	ojv4::u_p_status_enum new_status = ojv4::u_p_status_enum::TODO;

	try {
		new_status = query_user_s_problem_status_from_solution(conn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Query user's problem status failed!", e);
		throw;
	}

	if (new_status == ojv4::u_p_status_enum::TODO) {
		return;
	}

	ojv4::u_p_status_enum previous_status = ojv4::u_p_status_enum::TODO;
	{
		mysqlpp::Query query = conn.query(
				"select status from user_problem "
				"where u_id = %0 and p_id = %1 and c_id is null");
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(u_id, p_id);
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query previous status from user-problem failed!", e);
			throw e;
		}
		if (res.empty()) {
			previous_status = ojv4::u_p_status_enum::TODO;
		} else {
			previous_status = ojv4::u_p_status_enum(ojv4::u_p_status_type(res[0]["status"]));
		}
	}

	if (previous_status == ojv4::u_p_status_enum::TODO) {
		mysqlpp::Query insert = conn.query(
				"insert into user_problem (u_id, p_id, status, c_id) "
				"values (%0, %1, %2, null)"
		);
		insert.parse();

		mysqlpp::SimpleResult insert_res = insert.execute(u_id, p_id, ojv4::u_p_status_type(new_status));
		if (!insert_res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	} else if (previous_status != new_status) {
		mysqlpp::Query update = conn.query(
				"update user_problem set "
				"status = %0 "
				"where u_id = %1 and p_id = %2 and c_id is null"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(ojv4::u_p_status_type(new_status), u_id, p_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	}
}


