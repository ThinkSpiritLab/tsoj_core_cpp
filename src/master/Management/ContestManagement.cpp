/*
 * ContestManagement.cpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#include "ContestManagement.hpp"

#include "mysql_empty_res_exception.hpp"
#include "logger.hpp"
#include "SARecorder.hpp"

#include <unordered_map>
#include <mutex>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/variant.hpp>
#include <boost/format.hpp>
#include <kerbal/redis/redis_command.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/query.h>


extern std::ofstream log_fp;


void ContestManagement::refresh_all_problems_submit_and_accept_num_in_contest(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id)
{
	std::unordered_map<ojv4::p_id_type, ProblemSARecorder, id_type_hash<ojv4::p_id_type>> m;
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
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Query problems' submit and accept num in contest failed!", e, " ct_id: ", ct_id);
			throw e;
		}

		while (mysqlpp::Row row = solutions.fetch_row()) {
			auto it = row.begin();
			ojv4::u_id_type u_id(::atoll(it->c_str()));
			++it;
			ojv4::p_id_type p_id(::atoll(it->c_str()));
			++it;
			ojv4::s_result_enum s_result(ojv4::s_result_enum(::atoll(it->c_str())));
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
		mysqlpp::SimpleResult res = clear.execute(ct_id);
		if (!res) {
			MysqlEmptyResException e(clear.errnum(), clear.error());
			EXCEPT_FATAL(0, 0, log_fp, "Clear problems' submit and accept num in contest failed!", e, " ct_id: ", ct_id);
			throw e;
		}
	}

	mysqlpp::Query update = mysql_conn.query(
			"update contest_problem set ct_p_submit = %2, ct_p_accept = %3 "
			"where ct_id = %0 and p_id = %1"
	);
	update.parse();

	for (const auto & ele : m) {
		ojv4::p_id_type p_id = ele.first;
		const ProblemSARecorder & p_info = ele.second;
		int submit = p_info.submit_num();
		int accept = p_info.accept_num();

		mysqlpp::SimpleResult res = update.execute(ct_id, p_id, submit, accept);
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update problem's submit and accept num in contest failed!", e, " ct_id: ", ct_id, " p_id: ", p_id);
			throw e;
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

	mysqlpp::Query update = mysql_conn.query(
			"update contest_problem "
			"set ct_p_submit = %0, ct_p_accept = %1 "
			"where p_id = %2 and ct_id = %3"
	);
	update.parse();

	mysqlpp::SimpleResult update_res = update.execute(p_info.submit_num(), p_info.accept_num(), p_id, ct_id);
	if (!update_res) {
		MysqlEmptyResException e(update.errnum(), update.error());
		EXCEPT_FATAL(0, 0, log_fp, "Update problem submit and accept num failed!", e);
		throw e;
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
				case s_result_enum_type::ACCEPTED:
					is_ac = true;
					ac_time = row["s_posttime"];
					break;
				default:
					++error_count;
			}
		}
	}

	mysqlpp::Query query_if_exists = mysql_conn.query(
			"select id from contest_user_problem%0 where u_id = %1 and p_id = %2"
	);
	query_if_exists.parse();

	mysqlpp::StoreQueryResult query_if_exists_res = query_if_exists.store(ct_id, u_id, p_id);
	if (query_if_exists.errnum() != 0) {
		MysqlEmptyResException e(query_if_exists.errnum(), query_if_exists.error());
		EXCEPT_FATAL(0, 0, log_fp, "Query previous user problem status failed!", e);
		throw e;
	}

	if (query_if_exists_res.empty()) {

		mysqlpp::Query insert = mysql_conn.query(
				"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
				"values (%1, %2, %3, %4q, %5)"
		);
		insert.parse();

		mysqlpp::SimpleResult res = insert.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		if (!res) {
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

		mysqlpp::SimpleResult res = update.execute(ct_id, u_id, p_id, is_ac, ac_time, error_count);
		if (!res) {
			MysqlEmptyResException e(update.errnum(), update.error());
			EXCEPT_FATAL(0, 0, log_fp, "Update user problem status failed!", e);
			throw e;
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

void ContestManagement::update_scoreboard(mysqlpp::Connection & mysql_conn, kerbal::redis::RedisContext redis_conn, ojv4::ct_id_type ct_id)
{
	std::chrono::system_clock::time_point ct_starttime;
	std::chrono::system_clock::time_point ct_endtime;
	std::chrono::system_clock::time_point ct_lockranktime;
	std::chrono::seconds ct_timeplus(1200);
	int problem_count = 0;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select ct_starttime, ct_endtime, ct_timeplus, ct_lockrankbefore, count(p_id) as problem_count "
				"from contest, contest_problem "
				"where contest.ct_id = %0 and contest_problem.ct_id = %1"
		);
		query.parse();
		mysqlpp::StoreQueryResult res = query.store(ct_id, ct_id);
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Error occurred while getting contest details.", e);
			throw e;
		}
		if (res.size() == 0) {
			std::runtime_error e("A contest id doesn't exist");
			EXCEPT_FATAL(0, 0, log_fp, "Error occurred while getting contest details.", e);
			throw e;
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
				"select distinct u_id, u_name, u_realname from ctuser "
				"where ct_id = %0 order by u_id"
		);
		query.parse();
		mysqlpp::StoreQueryResult res = query.store(ct_id);
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Error occurred while getting contest details.", e);
			throw e;
		}
		for(const mysqlpp::Row & row : res) {
			ojv4::u_id_type u_id = row["u_id"];
			std::string u_name(row["u_name"]);
			std::string u_realname(row["u_realname"]);
			scoreboard.push_back(new scoreboard_row(u_id, std::move(u_name), std::move(u_realname)));
		}
	}

	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, p_logic, s_result, s_posttime "
				"from contest_solution%0 as so, contest_problem as cp "
				"where so.p_id = cp.p_id and cp.ct_id = %1 and "
				"u_id in (select u_id from ctuser where ct_id = %1) order by s_id"
		);
		query.parse();
		mysqlpp::StoreQueryResult res = query.store(ct_id, ct_id);
		if (query.errnum() != 0) {
			MysqlEmptyResException e(query.errnum(), query.error());
			EXCEPT_FATAL(0, 0, log_fp, "Error occurred while getting contest details.", e);
			throw e;
		}
		for (const auto & row : res) {
			ojv4::u_id_type u_id = row["u_id"];
			logic_p_id_type p_logic = row["p_logic"].c_str()[0];
			if (('A' <= p_logic && p_logic <= 'Z') == false) {
				continue;
			}
			auto s_result = ojv4::s_result_enum(ojv4::s_result_in_db_type(row["s_result"]));
			auto s_posttime = std::chrono::system_clock::from_time_t(time_t(mysqlpp::DateTime(row["s_posttime"])));

			// 通过 uid 二分搜索
			auto it = std::lower_bound(scoreboard.begin(), scoreboard.end(), u_id, [](const boost::variant<ojv4::u_id_type, scoreboard_row> & a, const boost::variant<ojv4::u_id_type, scoreboard_row> & b) {
				struct var_visitor : public boost::static_visitor<ojv4::u_id_type>
				{
					public:
						ojv4::u_id_type operator()(ojv4::u_id_type u_id) const
						{
							return u_id;
						}

						ojv4::u_id_type operator()(const scoreboard_row & src) const
						{
							return src.u_id;
						}
				} v;

				ojv4::u_id_type a_uid = boost::apply_visitor(v, a);
				ojv4::u_id_type b_uid = boost::apply_visitor(v, b);
				return a_uid.to_literal() < b_uid.to_literal();
			});

			if(it != scoreboard.end() && it->u_id == u_id) {
				it->solutions[p_logic].add_solution_with_lock(s_result, s_posttime, current_time, ct_starttime, ct_lockranktime, ct_endtime);
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
	for (const auto & row : scoreboard) {
		static boost::format row_templ(R"------([%d,%d,"%s","%s",[)------");
		scoreboard_s += (row_templ % rank++ % row.u_id % row.u_name % row.u_realname).str();

		const auto & solutions = row.solutions;
		for (const auto & p_u_p_status : solutions) {
			logic_p_id_type p_logic = p_u_p_status.first;
			const auto & u_p_status = p_u_p_status.second;
			switch (u_p_status.status) {
				case u_p_status::never_try:
				case u_p_status::error: {
					static boost::format accept_templ("[%d,1,%d],");
					scoreboard_s += (accept_templ % (p_logic - 'A') % u_p_status.tried_times).str();
					break;
				}
				case u_p_status::accept: {
					static boost::format accept_templ("[%d,0,%d,%d],");
					scoreboard_s += (accept_templ % (p_logic - 'A') % u_p_status.tried_times % u_p_status.ac_time_consume.count()).str();
					break;
				}
				case u_p_status::pending: {
					static boost::format accept_templ("[%d,2,%d],");
					scoreboard_s += (accept_templ % (p_logic - 'A') % u_p_status.tried_times).str();
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
	kerbal::redis::RedisCommand rset("set contest_scoreboard:%d %s");
	rset.execute(redis_conn, ct_id, scoreboard_s);
}



