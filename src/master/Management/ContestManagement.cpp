/*
 * ContestManagement.cpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#include "ContestManagement.hpp"
#include "SARecorder.hpp"

#include <string>
#include <unordered_map>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>

using namespace std::string_literals;

void ContestManagement::refresh_all_problems_submit_and_accept_num_in_contest(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id)
{
	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, ojv4::p_id_type::hash> m;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select s_id, u_id, p_id, s_result from contest_solution%0 "
				"where u_id in (select u_id from ctuser where ct_id = %1) order by s_id"
				// 注: 需要过滤掉不在竞赛中的用户 (考虑到有加人再删人的情形)
				// order by s_id 是重中之重, 因为根据 update 先后次序的不同, s_id 小的可能在 s_id 大的 solution 后面
		);
		query.parse();

		mysqlpp::UseQueryResult solutions = query.use(ct_id, ct_id);
		if (!solutions) {
			throw std::runtime_error("Query problems' solutions in contest failed!"s
					+ " ct_id = " + std::to_string(ct_id)
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
				"update contest_problem set c_p_submit = 0, c_p_accept = 0 "
				"where ct_id = %0"
		);
		clear.parse();
		if (!clear.execute(ct_id)) {
			throw std::runtime_error("Clear problems' submit and accept num in contest failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update contest_problem set ct_p_submit = %2, ct_p_accept = %3 "
			"where ct_id = %0 and p_id = %1"
	);
	update.parse();

	for (const auto & [p_id, p_sa_info] : m) {
		if (!update.execute(ct_id, p_id, p_sa_info.submit_num(), p_sa_info.accept_num())) {
			throw std::runtime_error("Update problem's submit and accept num in contest failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}

	trans.commit();
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

		mysqlpp::UseQueryResult solutions = query.use(ct_id, p_id);
		if (!solutions) {
			throw std::runtime_error("Query problem's solutions failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(query.errnum())
					+ " MySQL error: " + query.error());
		}

		while (::MYSQL_ROW row = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(row[0]));
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(row[1])));
			p_info.add_solution(u_id, s_result);
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update contest_problem "
			"set ct_p_submit = %0, ct_p_accept = %1 "
			"where p_id = %2 and ct_id = %3"
	);
	update.parse();

	if (!update.execute(p_info.submit_num(), p_info.accept_num(), p_id, ct_id)) {
		throw std::runtime_error("Update problem's submit and accept num failed!"s
				+ " ct_id = " + std::to_string(ct_id)
				+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
				+ " MySQL error: " + mysql_conn.error());
	}
}

void ContestManagement::update_user_problem_status(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
}

void ContestManagement::update_user_problem(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	//WARNING: 'is_first_ac' field has been marked as deprecated
	using s_result_in_db_type = ojv4::s_result_in_db_type;
	using s_result_enum_type = ojv4::s_result_enum;

	bool is_ac = false;
	mysqlpp::Null<mysqlpp::DateTime> ac_time;
	int error_count = 0;
	{
		mysqlpp::Query get_solution = mysql_conn.query(
				"select s_result, s_posttime from contest_solution%0 "
				"where u_id = %1 and p_id = %2 "
				"order by s_id"
		);
		get_solution.parse();

		mysqlpp::UseQueryResult solutions = get_solution.use(ct_id, u_id, p_id);

		if (!solutions) {
			throw std::runtime_error("Query solution failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}

		if (solutions.fetch_lengths() == 0) {
			return; // 如果 solution 为空, 即该用户没有过提交, 则放弃更新
		}

		while (mysqlpp::Row row = solutions.fetch_row()) {
			auto s_result(s_result_enum_type(boost::lexical_cast<s_result_in_db_type>(row["s_result"])));
			if (s_result == s_result_enum_type::ACCEPTED) {
				is_ac = true;
				ac_time = mysqlpp::DateTime(row["s_posttime"]);
				break; //
			} else if (s_result != s_result_enum_type::SYSTEM_ERROR) { // ignore system error
				++error_count;
			}
		}
	}

	bool if_exists = false;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select id from contest_user_problem%0 where u_id = %1 and p_id = %2"
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(ct_id, u_id, p_id);
		if (!res) {
			throw std::runtime_error("Query previous user problem failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
		if (res.empty()) {
			if_exists = false;
		} else {
			if_exists = true;
		}
	}

	if (if_exists == false) {
		mysqlpp::Query insert = mysql_conn.query(
				"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
				"values (%1, %2, %3, %4q, %5)"
		);
		insert.parse();

		mysqlpp::SimpleResult res = insert.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		if (!res) {
			throw std::runtime_error("Update user problem failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	} else {
		mysqlpp::Query update = mysql_conn.query(
				"update contest_user_problem%0 set "
				"is_ac = %3, ac_time = %4q, error_count = %5 "
				"where u_id = %1 and p_id = %2"
		);
		update.parse();

		mysqlpp::SimpleResult res = update.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		if (!res) {
			throw std::runtime_error("Update user problem status failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(mysql_conn.errnum())
					+ " MySQL error: " + mysql_conn.error());
		}
	}
}

struct u_p_status
{
		int tried_times;

		enum {
			never_try,
			error,
			accept,
			pending,
		} status;

		std::chrono::seconds ac_time_consume;

		u_p_status() : tried_times(0), status(never_try), ac_time_consume(0)
		{
		}

		void add_solution(ojv4::s_result_enum s_result,
				std::chrono::system_clock::time_point post_time,
				std::chrono::system_clock::time_point ct_starttime)
		{
			if (s_result == ojv4::s_result_enum::SYSTEM_ERROR) {
				return;
			}
			switch (status) {
				case never_try:
				case error: { // 封榜前没 A
					if (s_result == ojv4::s_result_enum::ACCEPTED) {
						status = accept;
						ac_time_consume = std::chrono::duration_cast<std::chrono::seconds>(post_time - ct_starttime);
					} else {
						status = error;
					}
					++tried_times;
					break;
				}
				case accept: // 之前已 A 则忽略
					break;
				case pending:
					++tried_times;
					break;
			}
		}

		void add_solution_with_lock(ojv4::s_result_enum s_result,
				std::chrono::system_clock::time_point post_time,
				std::chrono::system_clock::time_point current_time,
				std::chrono::system_clock::time_point ct_starttime,
				std::chrono::system_clock::time_point ct_lockranktime,
				std::chrono::system_clock::time_point ct_endtime)
		{
			if (s_result == ojv4::s_result_enum::SYSTEM_ERROR) {
				return;
			}
			switch (status) {
				case never_try:
				case error: { // 封榜前没 A
					bool is_submit_during_scoreboard_closed = false;
					if (current_time > ct_endtime) {
						is_submit_during_scoreboard_closed = false;
					} else {
						is_submit_during_scoreboard_closed = ct_lockranktime < post_time && post_time < ct_endtime;
					}

					if (is_submit_during_scoreboard_closed == true) { // 如果是在封榜时间段提交的, 无论对不对, 都做模糊处理, 即通过数始终 + 1
						status = pending;
					} else {
						if (s_result == ojv4::s_result_enum::ACCEPTED) {
							status = accept;
							ac_time_consume = std::chrono::duration_cast<std::chrono::seconds>(post_time - ct_starttime);
						} else {
							status = error;
						}
					}
					++tried_times;
					break;
				}
				case accept: // 之前已 A 则忽略
					break;
				case pending:
					++tried_times;
					break;
			}
		}
};

typedef char logic_p_id_type;


class scoreboard_row
{
	public:
		const ojv4::u_id_type u_id;
		const std::string u_name;
		const std::string u_realname;

		std::map<logic_p_id_type, u_p_status> solutions;

		scoreboard_row(ojv4::u_id_type u_id,
				const std::string & u_name,
				const std::string & u_realname) :
					u_id(u_id), u_name(u_name), u_realname(u_realname), solutions()
		{
		}

		scoreboard_row(ojv4::u_id_type u_id,
				std::string && u_name,
				std::string && u_realname) :
					u_id(u_id), u_name(u_name), u_realname(u_realname), solutions()
		{
		}

		int get_accept_num() const
		{
			int count = 0;
			for (auto it = solutions.cbegin(); it != solutions.cend(); ++it) {
				if (it->second.status == u_p_status::accept) {
					++count;
				}
			}
			return count;
		}

		int get_tried_times() const
		{
			int count = 0;
			for (auto it = solutions.cbegin(); it != solutions.cend(); ++it) {
				count += it->second.tried_times;
			}
			return count;
		}

		std::chrono::seconds get_total_time(std::chrono::seconds time_plus) const
		{
			std::chrono::seconds total_time(0);
			for (auto it = solutions.cbegin(); it != solutions.cend(); ++it) {
				if (it->second.status == u_p_status::accept) {
					total_time += it->second.ac_time_consume + time_plus * (it->second.tried_times - 1);
				}
			}
			return total_time;
		}

};

void ContestManagement::update_scoreboard(mysqlpp::Connection & mysql_conn, kerbal::redis_v2::connection & redis_conn, ojv4::ct_id_type ct_id)
{
	std::chrono::system_clock::time_point ct_starttime;
	std::chrono::system_clock::time_point ct_endtime;
	std::chrono::system_clock::time_point ct_lockranktime;
	std::chrono::seconds ct_timeplus(1200);
	int problem_count = 0;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select ct_starttime, ct_endtime, ct_timeplus, ct_lockrankbefore, "
				"(select count(p_id) from contest_problem where contest_problem.ct_id = contest.ct_id) as problem_count "
				"from contest "
				"where ct_id = %0"
		);
		query.parse();
		mysqlpp::StoreQueryResult res = query.store(ct_id);
		if (query.errnum() != 0) {
			throw std::runtime_error("Query contest details failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(query.errnum())
					+ " MySQL error: " + query.error());
		}

		if (res.empty()) {
			return; // 不存在的竞赛
		}

		const mysqlpp::Row & ct_info = res[0];
		ct_starttime = std::chrono::system_clock::from_time_t(time_t(mysqlpp::DateTime(ct_info["ct_starttime"])));
		ct_endtime = std::chrono::system_clock::from_time_t(time_t(mysqlpp::DateTime(ct_info["ct_endtime"])));
		if (ct_info["ct_timeplus"] != mysqlpp::null) {
			ct_timeplus = std::chrono::seconds(int(ct_info["ct_timeplus"]));
		}
		std::chrono::seconds ct_lockrankbefore(3600);
		if (ct_info["ct_lockrankbefore"] != mysqlpp::null) {
			ct_lockrankbefore = std::chrono::seconds(int(ct_info["ct_lockrankbefore"]));
		}
		ct_lockranktime = ct_endtime - ct_lockrankbefore;
		problem_count = ct_info["problem_count"];
	}

	std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();

	boost::ptr_vector<scoreboard_row> scoreboard;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, u_name, u_realname from ctuser "
				"where ct_id = %0 order by u_id" // 注意，这里的 order by u_id 的目的是为了方便下面的二分查找
		);
		query.parse();
		mysqlpp::UseQueryResult users = query.use(ct_id);
		if (!users) {
			throw std::runtime_error("Query contest uses' information failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(query.errnum())
					+ " MySQL error: " + query.error());
		}
		scoreboard.reserve(100);
		while (::MYSQL_ROW user = users.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(user[0]));
			const char * u_name = user[1];
			const char * u_realname = user[2];
			scoreboard.push_back(new scoreboard_row(u_id, u_name, u_realname));
		}
	}
	
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, (select p_logic from contest_problem where ct_id = %0 and contest_problem.p_id = so.p_id) as p_logic, s_result, s_posttime "
				"from contest_solution%1 as so "
				"order by s_id"
		);
		query.parse();
		mysqlpp::UseQueryResult solutions = query.use(ct_id, ct_id);
		if (!solutions) {
			throw std::runtime_error("Query contest solutions failed!"s
					+ " ct_id = " + std::to_string(ct_id)
					+ " MySQL errnum: " + std::to_string(query.errnum())
					+ " MySQL error: " + query.error());
		}
		while (::MYSQL_ROW solution = solutions.fetch_raw_row()) {
			auto u_id(boost::lexical_cast<ojv4::u_id_type>(solution[0]));
			if (solution[1] == nullptr) {
				continue; // 过滤掉已删除，造成 p_logic 取出为 NULL 的题
			}
			logic_p_id_type p_logic = solution[1][0];
			if (('A' <= p_logic && p_logic <= 'Z') == false) {
				continue;
			}
			auto s_result(ojv4::s_result_enum(boost::lexical_cast<int>(solution[2])));
			const char* s_posttime_s = solution[3];
			auto s_posttime = std::chrono::system_clock::from_time_t(time_t(
													mysqlpp::DateTime(s_posttime_s)));

			// 二分查找 u_id = u_id 的 scoreboard_row
			auto l = scoreboard.begin();
			auto r = scoreboard.end();
			auto it = l + (r - l) / 2;
//			std::cout << "要找 " << u_id << endl;
			while (l <= r && l != scoreboard.end() && r != scoreboard.begin()) {
//				std::cout << "当前 " << it->u_id << std::endl;
				if (it->u_id.to_literal() < u_id.to_literal()) {
					l = it;
					++l;
					it = l + (r - l) / 2;
					continue;
				} else if (it->u_id.to_literal() > u_id.to_literal()) {
					r = it;
					it = l + (r - l) / 2;
					continue;
				} else {
//					std::cout << "命中" << endl;
					it->solutions[p_logic].add_solution_with_lock(s_result, s_posttime, current_time, ct_starttime, ct_lockranktime, ct_endtime);
					break;
				}
			}
		}
	}

	scoreboard.sort([ct_timeplus](const scoreboard_row & a, const scoreboard_row & b) {
		int a_ac_num = a.get_accept_num();
		int b_ac_num = b.get_accept_num();
		if (a_ac_num != b_ac_num) { // 当通过数不同时, 通过题数多者优先
			return a_ac_num > b_ac_num;
		}
		if (a_ac_num != 0) { // 当两者通过数相等且均不为 0 时, 用时少者优先
			return a.get_total_time(ct_timeplus) < b.get_total_time(ct_timeplus);
		}
		// 当两者通过数均为 0 时, 根据尝试次数排名
		int a_tried_times = a.get_tried_times();
		int b_tried_times = b.get_tried_times();
		// 尝试次数相同, 按 u_id 排
		if (a_tried_times == b_tried_times) {
			return a.u_id.to_literal() < b.u_id.to_literal();
		}
		if (a_tried_times == 0) {
			return false;
		}
		if (b_tried_times == 0) {
			return true;
		}
		return a_tried_times < b_tried_times;
	});

	int rank = 1;
	std::string scoreboard_s = "[" + std::to_string(problem_count) + ",";
	scoreboard_s.reserve(scoreboard.size() * 60);
	for (const auto & row : scoreboard) {
		static boost::format row_templ(R"------([%d,%d,"%s","%s",[)------");
		scoreboard_s += (boost::format(row_templ) % rank++ % row.u_id % row.u_name % row.u_realname).str();

		const auto & solutions = row.solutions;
		for (const auto & p_u_p_status : solutions) {
			logic_p_id_type p_logic = p_u_p_status.first;
			const auto & u_p_status = p_u_p_status.second;
			switch (u_p_status.status) {
				case u_p_status::never_try:
				case u_p_status::error: {
					static boost::format accept_templ("[%d,1,%d],");
					scoreboard_s += (boost::format(accept_templ) % (p_logic - 'A') % u_p_status.tried_times).str();
					break;
				}
				case u_p_status::accept: {
					static boost::format accept_templ("[%d,0,%d,%d],");
					scoreboard_s += (boost::format(accept_templ) % (p_logic - 'A') % u_p_status.tried_times % u_p_status.ac_time_consume.count()).str();
					break;
				}
				case u_p_status::pending: {
					static boost::format accept_templ("[%d,2,%d],");
					scoreboard_s += (boost::format(accept_templ) % (p_logic - 'A') % u_p_status.tried_times).str();
					break;
				}
			}
		}
		if (solutions.size() != 0) {
			scoreboard_s.pop_back();
		}
		scoreboard_s += "]],";
	}
	if (scoreboard.size() != 0) {
		scoreboard_s.pop_back();
	}
	scoreboard_s += "]";
	
	//cout << scoreboard_s << endl;
	redis_conn.execute("set contest_scoreboard:%d %s", ct_id.to_literal(), scoreboard_s.c_str());
}



