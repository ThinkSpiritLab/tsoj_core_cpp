/*
 * ContestManagement.cpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#include "ContestManagement.hpp"
#include "mysql_empty_res_exception.hpp"
#include "logger.hpp"
#include "sync_instance_pool.hpp"
#include "SARecorder.hpp"

#include <unordered_map>
#include <future>
#include <mutex>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>


extern std::ofstream log_fp;


void ContestManagement::refresh_all_problems_submit_and_accept_num_in_contest(ojv4::ct_id_type ct_id)
{
	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	auto conn = conn_pool::fetch();

	mysqlpp::Query query = conn->query(
			"select s_id, u_id, p_id, s_result from contest_solution%0 "
			"where u_id in (select u_id from ctuser where ct_id = %1) order by s_id"
			// 注: 需要过滤掉不在竞赛中的用户 (考虑到有加人再删人的情形)
			// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
	);
	query.parse();

	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, id_type_hash<ojv4::p_id_type>> m;
	{
		mysqlpp::StoreQueryResult solutions = query.store(ct_id, ct_id);
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

		auto as = [](conn_pool::auto_revert_t conn, ojv4::ct_id_type ct_id, ojv4::p_id_type p_id, int submit, int accept) -> mysqlpp::SimpleResult
		{
			mysqlpp::Query update = conn->query(
					"update contest_problem set ct_p_submit = %2, ct_p_accept = %3 "
					"where ct_id = %0 and p_id = %1"
			);
			update.parse();
			mysqlpp::SimpleResult res = update.execute(ct_id, p_id, submit, accept);
			if(!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				throw e;
			}
			return res;
		};
		conn_pool::auto_revert_t conn = conn_pool::block_fetch();
		future_group.emplace_back(p_id, std::async(std::launch::async, as, std::move(conn), ct_id, p_id, submit, accept));
	}

	std::exception_ptr e_ptr;
	for(auto & ele : future_group) {
		ojv4::p_id_type p_id = ele.first;
		try {
			mysqlpp::SimpleResult res = ele.second.get();
		} catch(const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Refresh all problems' submit and accept num in contest failed!", e, " ct_id: ", ct_id, " p_id: ", p_id);
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


void ContestManagement::update_problem_s_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::p_id_type p_id)
{
	ProblemSARecorder p_info;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, s_result from contest_solution%0 "
				"where p_id = %1 and u_id in (select u_id from ctuser where ct_id = %0) order by s_id"
				// 需要排除掉已不在竞赛中的用户
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(ct_id, p_id);

		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query problem's solutions failed!", e);
			throw e;
		}

		for (const auto & row : res) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			p_info.add_solution(u_id, s_result);
		}
	}

	{
		mysqlpp::Query update = mysql_conn.query(
				"update contest_problem set ct_p_submit = %0, ct_p_accept = %1 where p_id = %2 and ct_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(p_info.submit_num(), p_info.accept_num(), p_id, ct_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update problem submit and accept num failed!", e);
			throw e;
		}
	}
}

void ContestManagement::update_user_problem_status(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
//	constexpr ojv4::s_result_in_db_type ACCEPTED = ojv4::s_result_in_db_type(ojv4::s_result_enum::ACCEPTED);
//	constexpr ojv4::s_result_in_db_type SYSTEM_ERROR = ojv4::s_result_in_db_type(ojv4::s_result_enum::SYSTEM_ERROR);
//
//	mysqlpp::Query insert = mysql_conn.query(
//			"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
//			"values (%1, %2, "
//			"((select count(s_id) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result = %3) > 0), " // is_ac
//			"(select min(s_posttime) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result = %3), " // ac_time
//			"(select count(s_id) from (select s_id from contest_solution%0 where u_id = %1 and p_id = %2 and s_result <> %4 order by s_id) as ctusr_pro_status \
//						where \
//						s_id >= (select min(s_id) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result <> %4) \
//						and \
//						s_id < (select min(s_id) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result = %3)) "
//			")"
//	);
//	insert.parse();
//	if (!insert.execute(ct_id, u_id, p_id, ACCEPTED, SYSTEM_ERROR)) {
//		mysqlpp::Query update = mysql_conn.query(
//				"update contest_user_problem%0 set "
//				"is_ac = ((select count(s_id) from contest_user_problem_solution%0 where u_id = %1 and p_id = %2 and s_result = %3) > 0), " // is_ac
//				"ac_time = (select min(s_posttime) from contest_user_problem_solution%0 where u_id = %1 and p_id = %2 and s_result = %3), " // ac_time
//				"error_count = (select count(s_id) from (select s_id from contest_solution%0 where u_id = %1 and p_id = %2 and s_result <> %4 order by s_id) as ctusr_pro_status \
//								where \
//								s_id >= (select min(s_id) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result <> %4) \
//								and \
//								s_id < (select min(s_id) from contest_solution%0 where u_id = %1 and p_id = %2 and s_result = %3)) "
//				"where u_id = %1 and p_id = %2"
//		);
//		update.parse();
//		if(!update.execute(ct_id, u_id, p_id, ACCEPTED, SYSTEM_ERROR)) {
//			MysqlEmptyResException e(mysql_conn.errnum(), mysql_conn.error());
//			throw e;
//		}
//	}

}

void ContestManagement::update_user_problem(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	using s_result_in_db_type = ojv4::s_result_in_db_type;
	using s_result_enum_type = ojv4::s_result_enum;

	bool is_ac = false;
	mysqlpp::DateTime ac_time;
	int error_count = 0;
	{
		mysqlpp::Query get_solution = mysql_conn.query(
				"select s_result, s_posttime from contest_solution%0 "
				"where u_id = %1 and p_id = %2"
		);
		get_solution.parse();

		mysqlpp::StoreQueryResult solutions = get_solution.store(ct_id, u_id, p_id);

		if (get_solution.errnum() != 0) {
			MysqlEmptyResException e(get_solution.errnum(), get_solution.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query solution failed!", e);
			throw e;
		}

		if(solutions.empty()) {
			return;
		}

		for (const auto & row : solutions) {
			switch (s_result_enum_type(s_result_in_db_type(row["s_result"]))) {
				case s_result_enum_type::SYSTEM_ERROR: // ignore system error
					break;
				case s_result_enum_type::ACCEPTED: {
					is_ac = true;
					ac_time = row["s_posttime"];
					break;
				}
				default: {
					++error_count;
				}
			}
		}
	}

	mysqlpp::Query query_if_exists = mysql_conn.query(
			"select id from contest_user_problem%0 where u_id = %1 and p_id = %2"
	);
	query_if_exists.parse();

	static std::mutex mtx_of_updating_ctup;
	std::lock_guard<std::mutex> lck_g(mtx_of_updating_ctup);

	mysqlpp::StoreQueryResult query_if_exists_res = query_if_exists.store(ct_id, u_id, p_id);
	if (query_if_exists.errnum() != 0) {
		MysqlEmptyResException e(query_if_exists.errnum(), query_if_exists.error());
		EXCEPT_FATAL(0, 0, log_fp, "Update user problem status failed!", e);
		throw e;
	}

	if (query_if_exists_res.empty()) {

		mysqlpp::Query insert = mysql_conn.query(
				"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
				"values (%1, %2, %3, %4q, %5)"
		);
		insert.parse();

		mysqlpp::SimpleResult insert_res = insert.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		if (!insert_res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user problem status failed!", e);
			throw e;
		}
	} else {
		mysqlpp::Query update = mysql_conn.query(
				"update contest_user_problem%0 set "
				"is_ac = %3, " // is_ac
				"ac_time = %4q, " // ac_time
				"error_count = %5 "
				"where u_id = %1 and p_id = %2"
		);
		update.parse();

		mysqlpp::SimpleResult update_res;
		try {
			update_res = update.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Update user problem status failed!", e);
			throw e;
		}

		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user problem status failed!", e);
			throw e;
		}
	}
}


