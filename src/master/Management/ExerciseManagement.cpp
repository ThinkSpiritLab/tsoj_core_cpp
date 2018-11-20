/*
 * ExerciseManagement.cpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#include "ExerciseManagement.hpp"
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


void ExerciseManagement::refresh_all_users_submit_and_accept_num()
{
	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	std::unordered_map<ojv4::u_id_type, UserSARecorder, id_type_hash<ojv4::u_id_type>> m;
	{
		auto conn = conn_pool::fetch();

		mysqlpp::Query query = conn->query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();
		mysqlpp::StoreQueryResult solutions = query.store();
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

		auto as = [](conn_pool::auto_revert_t conn, ojv4::u_id_type u_id, int submit, int accept) -> mysqlpp::SimpleResult
		{
			mysqlpp::Query update = conn->query(
					"update user set u_submit = %1, u_accept = %2 "
					"where u_id = %0"
			);
			update.parse();
			mysqlpp::SimpleResult res = update.execute(u_id, submit, accept);
			if(!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				throw e;
			}
			return res;
		};
		conn_pool::auto_revert_t conn = conn_pool::block_fetch();
		future_group.emplace_back(u_id, std::async(std::launch::async, as, std::move(conn), u_id, submit, accept));
	}

	std::exception_ptr e_ptr;
	for(auto & ele : future_group) {
		ojv4::u_id_type u_id = ele.first;
		try {
			mysqlpp::SimpleResult res = ele.second.get();
		} catch(const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Refresh all users' submit and accept num failed!", e, " u_id: ", u_id);
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

void ExerciseManagement::refresh_all_problems_submit_and_accept_num()
{
	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, id_type_hash<ojv4::p_id_type>> m;
	{
		auto conn = conn_pool::fetch();
		mysqlpp::Query query = conn->query(
				"select u_id, p_id, s_result from solution "
				"order by s_id"
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();
		mysqlpp::StoreQueryResult solutions = query.store();

		if(query.errnum() != 0) {
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

	std::vector<std::pair<ojv4::p_id_type, std::future<mysqlpp::SimpleResult>>> future_group;

	for (const auto & ele : m) {
		ojv4::p_id_type p_id = ele.first;
		const ProblemSARecorder & p_info = ele.second;
		int submit = p_info.submit_num();
		int accept = p_info.accept_num();

		static auto as = [](conn_pool::auto_revert_t conn, ojv4::p_id_type p_id, int submit, int accept) -> mysqlpp::SimpleResult
		{
			mysqlpp::Query update = conn->query(
					"update problem set p_submit = %1, p_accept = %2 "
					"where p_id = %0"
			);
			update.parse();
			mysqlpp::SimpleResult res = update.execute(p_id, submit, accept);
			if(!res) {
				MysqlEmptyResException e(update.errnum(), update.error());
				throw e;
			}
			return res;
		};
		conn_pool::auto_revert_t conn = conn_pool::block_fetch();
		future_group.emplace_back(p_id, std::async(std::launch::async, as, std::move(conn), p_id, submit, accept));
	}

	std::exception_ptr e_ptr;
	for(auto & ele : future_group) {
		ojv4::p_id_type p_id = ele.first;
		try {
			mysqlpp::SimpleResult res = ele.second.get();
		} catch(const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Refresh problem's submit and accept num in exercise failed!", e, " p_id: ", p_id);
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

void ExerciseManagement::refresh_all_user_problem()
{
	using conn_pool = sync_instance_pool<mysqlpp::Connection>;

	struct pair_hash
	{
			size_t operator()(const std::pair<ojv4::u_id_type, ojv4::p_id_type> & p) const
			{
				return id_type_hash<ojv4::u_id_type>()(p.first) ^ id_type_hash<ojv4::p_id_type>()(p.second);
			}
	};

	std::unordered_map<std::pair<ojv4::u_id_type, ojv4::p_id_type>, ojv4::u_p_status_enum, pair_hash> u_p_status;
	{
		auto conn = conn_pool::fetch();

		mysqlpp::Query query = conn->query(
				"select u_id, p_id, s_result from solution order by s_id"
		);
		query.parse();
		mysqlpp::StoreQueryResult solutions = query.store();

		if(query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query solutions failed!", e);
			throw e;
		}

		for(const auto & row : solutions) {
			ojv4::u_id_type u_id = row["u_id"];
			ojv4::p_id_type p_id = row["p_id"];
			ojv4::s_result_enum s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			switch(s_result) {
				case ojv4::s_result_enum::SYSTEM_ERROR:
					break;
				case ojv4::s_result_enum::ACCEPTED:
					u_p_status[{u_id, p_id}] = ojv4::u_p_status_enum::ACCEPTED;
					break;
				default: {
					auto it = u_p_status.find({u_id, p_id});
					if(it == u_p_status.end()) {
						u_p_status.insert({{u_id, p_id}, ojv4::u_p_status_enum::ATTEMPTED});
					}
				}
			}
		}
	}

	LOG_INFO(0, 0, log_fp, "User problem size: ", u_p_status.size());

	{
		auto conn = conn_pool::fetch();
		mysqlpp::Query delete_table = conn->query(
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
		auto conn = conn_pool::fetch();
		mysqlpp::Query insert = conn->query(
				"insert into user_problem(u_id, p_id, status, c_id) values"
		);

		for(auto it = u_p_status.cbegin(); it != u_p_status.cend(); ++it) {
			ojv4::u_id_type u_id = it->first.first;
			ojv4::p_id_type p_id = it->first.second;
			ojv4::u_p_status_enum status = it->second;
			insert << "(" << u_id << ',' << p_id << ',' << int(ojv4::u_p_status_type(status)) << ",null)";

			if (std::next(it) != u_p_status.cend()) {
				insert << ',';
			}
		}

		mysqlpp::SimpleResult res = insert.execute();
		if(!res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Insert failed!", e);
			throw e;
		}
	}

//	std::exception_ptr e_ptr = nullptr;
//	auto fu_wait_process = [&e_ptr](std::future<mysqlpp::SimpleResult> & fu) {
//		try {
//			fu.get();
//		} catch(const std::tuple<ojv4::u_id_type, ojv4::p_id_type, std::exception_ptr> & e) {
//			ojv4::u_id_type u_id = std::get<0>(e);
//			ojv4::p_id_type p_id = std::get<1>(e);
//			std::exception_ptr current_e_ptr = std::get<2>(e);
//
//			try { // 这段 try - catch 将 current_e_ptr 保存的异常扔了再接只是为了输出异常里的信息
//				std::rethrow_exception(current_e_ptr);
//			} catch(const std::exception & e) {
//				EXCEPT_FATAL(0, 0, log_fp, "Refresh user_problem failed!", e, " u_id: ", u_id, " p_id: ", p_id);
//				current_e_ptr = std::current_exception();
//			} catch(...) {
//				UNKNOWN_EXCEPT_FATAL(0, 0, log_fp, "Refresh user_problem failed!", " u_id: ", u_id, " p_id: ", p_id);
//				current_e_ptr = std::current_exception();
//			}
//
//			if(e_ptr == nullptr) {
//				e_ptr = current_e_ptr; // 只保留第一个 task 中产生的异常
//			}
//		}
//	};
//
//	int Interrupt = 0;
//
//	std::deque<std::future<mysqlpp::SimpleResult>> future_group;
//	for(auto it = u_p_status.cbegin(); it != u_p_status.cend(); ++it) {
//
//		if(future_group.size() > 1000) {
//			++Interrupt;
//			while(future_group.size() > 5) {
//				fu_wait_process(future_group.front());
//				future_group.pop_front();
//			}
//		}
//
//		ojv4::u_id_type u_id = it->first.first;
//		ojv4::p_id_type p_id = it->first.second;
//		ojv4::u_p_status_enum status = it->second;
//		static auto as = [](conn_pool::auto_revert_t conn,
//								ojv4::u_id_type u_id,
//								ojv4::p_id_type p_id,
//								ojv4::u_p_status_enum status) -> mysqlpp::SimpleResult
//		{
//			try {
//				mysqlpp::Query insert = conn->query(
//						"insert into user_problem(u_id, p_id, status, c_id) "
//						"values(%0, %1, %2, null)"
//				);
//				insert.parse();
//				mysqlpp::SimpleResult res = insert.execute(u_id, p_id, ojv4::u_p_status_type(status));
//				if(!res) {
//					MysqlEmptyResException e(insert.errnum(), insert.error());
//					throw e;
//				}
//				return res;
//			} catch(...) {
//				throw std::make_tuple(u_id, p_id, std::current_exception());
//			}
//		};
//		auto conn = conn_pool::block_fetch();
//		future_group.push_back(std::async(std::launch::async, as, std::move(conn), u_id, p_id, status));
//	}
//
//	LOG_INFO(0, 0, log_fp, "Interrupt: ", Interrupt);
//
//	for(auto & fu : future_group) {
//		fu_wait_process(fu);
//	}
//
//	if(e_ptr != nullptr) {
//		std::rethrow_exception(e_ptr);
//	}
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


	{
		mysqlpp::Query update = conn.query(
				"update user set u_submit = %1, u_accept = %2 "
				"where u_id = %0"
		);
		update.parse();
		mysqlpp::SimpleResult res;
		try {
			res = update.execute(u_id, u_info.submit_num(), u_info.accept_num());
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num failed!", e, " u_id: ", u_id);
			throw;
		}
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user's submit and accept num failed!", e, " u_id: ", u_id);
			throw e;
		}
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

	{
		mysqlpp::Query update = conn.query(
				"update problem set p_submit = %1, p_accept = %2 "
				"where p_id = %0"
		);
		update.parse();
		mysqlpp::SimpleResult res;
		try {
			res = update.execute(p_id, p_info.submit_num(), p_info.accept_num());
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num failed!", e, " p_id: ", p_id);
			throw;
		}
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num failed!", e, " p_id: ", p_id);
			throw e;
		}
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
//	/*
//	 insert into user_problem_status(u_id, p_id, status)
//	 values(1, 1001, 0)
//	 on duplicate key update status = 0
//	 */
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
//	if(!res) {
//		MysqlEmptyResException e(on_duplicate_insert.errnum(), on_duplicate_insert.error());
//		EXCEPT_FATAL(0, 0, log_fp, "Update user's problem status failed!", e );
//		throw e;
//	}
}

void ExerciseManagement::update_user_problem(mysqlpp::Connection & conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	ojv4::u_p_status_enum status = ojv4::u_p_status_enum::TODO;

	try {
		status = query_user_s_problem_status_from_solution(conn, u_id, p_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Query user's problem status failed!", e);
		throw;
	}

	if (status == ojv4::u_p_status_enum::TODO) {
		return;
	}

	bool if_exists = false;
	{
		mysqlpp::Query query_if_exists = conn.query(
				"select id from user_problem "
				"where u_id = %0 and p_id = %1 and c_id is null"
		);
		query_if_exists.parse();

		mysqlpp::StoreQueryResult query_if_exists_res = query_if_exists.store(u_id, p_id);
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
				"values (%0, %1, %2, null)"
		);
		insert.parse();

		mysqlpp::SimpleResult insert_res = insert.execute(u_id, p_id, ojv4::u_p_status_type(status));
		if (!insert_res) {
			MysqlEmptyResException e(insert.errnum(), insert.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	} else {
		mysqlpp::Query update = conn.query(
				"update user_problem set "
				"status = %0 "
				"where u_id = %1 and p_id = %2 and c_id is null"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(ojv4::u_p_status_type(status), u_id, p_id);
		if (!update_res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user-problem failed!", e);
			throw e;
		}
	}
}


