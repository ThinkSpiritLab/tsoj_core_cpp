/*
 * UpdateContestScoreboard.cpp
 *
 *  Created on: 2018年10月16日
 *      Author: peter
 */

#include <vector>
#include <set>
#include <algorithm>
#include <chrono>

#include <boost/format.hpp>
using boost::format;

#include <kerbal/redis/operation.hpp>

#include "UpdateContestScoreboard.hpp"
#include "united_resource.hpp"
#include "logger.hpp"

extern std::ofstream log_fp;

class contest
{
	private:
		struct user_problem
		{
				int tried_times;
				std::chrono::seconds first_ac_time_since_ct_start;

				enum
				{
					solved, ///< 已 A
					tried_incorrect_or_never_tried, ///< 封榜前没 A
					tried_pending ///< 封榜前没 A 且在封榜后又尝试了
				} status;

				user_problem() :
						tried_times(0), first_ac_time_since_ct_start(0), status(tried_incorrect_or_never_tried)
				{
				}
		};


		class contest_user
		{
			public:
				const int u_id;
				const std::string u_name;
				const std::string u_realname;

			private:
				mutable std::vector<user_problem> list;
				friend class contest;

			public:
				contest_user(int u_id, const std::string & u_name, const std::string & u_realname, int problem_num) :
						u_id(u_id), u_name(u_name), u_realname(u_realname), list(problem_num)
				{
				}

				int get_solved_num() const
				{
					int solved = 0;
					for (const auto & ele : list) {
						if (ele.status == user_problem::solved) {
							++solved;
						}
					}
					return solved;
				}

				std::chrono::seconds get_total_time(std::chrono::seconds time_plus) const
				{
					std::chrono::seconds total_time(0);
					for (const auto & ele : list) {
						if (ele.status == user_problem::solved) {
							total_time += time_plus * (ele.tried_times - 1);
							total_time += ele.first_ac_time_since_ct_start;
						}
					}
					return total_time;
				}

				void add_solution(char logic_pid, UnitedJudgeResult result, std::chrono::seconds posttime_since_ct_start,
						bool is_submit_during_scoreboard_closed) const
				{
					unsigned int this_logic_pid = logic_pid - 'A';
					if (this_logic_pid >= list.size()) {
						return;
					}
					user_problem & user_problem = list[this_logic_pid];

					switch (user_problem.status) {
						case user_problem::solved: // 之前已 A 则忽略
							break;
						case user_problem::tried_incorrect_or_never_tried: { // 封榜前没 A
							if (is_submit_during_scoreboard_closed == true) { // 如果是在封榜时间段提交的, 无论对不对, 都做模糊处理, 即通过数始终 + 1
								user_problem.status = user_problem::tried_pending;
								++user_problem.tried_times;
							} else {
								switch (result) {
									case UnitedJudgeResult::SYSTEM_ERROR: { // *** 忽略掉 system error 的情况
										break;
									}
									case UnitedJudgeResult::ACCEPTED: {
										user_problem.status = user_problem::solved;
										user_problem.first_ac_time_since_ct_start = posttime_since_ct_start;
										++user_problem.tried_times;
										break;
									}
									default:
										++user_problem.tried_times;
										break;
								}
							}
							break;
						}
						case user_problem::tried_pending: {
							if (result != UnitedJudgeResult::SYSTEM_ERROR) { // *** 忽略掉 system error 的情况
								++user_problem.tried_times;
							}
							break;
						}
					}
				}

				struct sort_by_u_id
				{
						bool operator()(const contest_user & a, const contest_user & b) const
						{
							return a.u_id < b.u_id;
						}
				};

				struct sort_by_acm_rank_rules
				{
					private:
						std::chrono::seconds time_plus;

					public:
						sort_by_acm_rank_rules(std::chrono::seconds time_plus) :
								time_plus(time_plus)
						{
						}

						bool operator()(const contest_user & a, const contest_user & b) const
						{
							int a_ac_num = a.get_solved_num();
							int b_ac_num = b.get_solved_num();
							if (a_ac_num == b_ac_num) {
//								if(a_ac_num == 0) {
//									return a.
//								}
								return a.get_total_time(time_plus) < b.get_total_time(time_plus);
							}
							return a_ac_num > b_ac_num;
						}
				};

		};


		const int ct_id;
		std::unique_ptr<mysqlpp::Connection> mysqlConn;
		int problem_num;
		std::chrono::system_clock::time_point ct_starttime;
		std::chrono::system_clock::time_point ct_endtime;
		std::chrono::seconds ct_timeplus;
		std::chrono::seconds ct_lockrankbefore;
		std::set<contest_user, contest_user::sort_by_u_id> user_set;

		static std::chrono::system_clock::time_point mysqlpp_datetime_to_std_timepoint(const mysqlpp::DateTime & src)
		{
			return std::chrono::system_clock::from_time_t((time_t)(src));
		}

		int get_problem_num(int ct_id)
		{
			mysqlpp::Query query_solution = mysqlConn->query(
					"select count(p_id) "
					"from contest_problem where ct_id = %0");
			query_solution.parse();
			mysqlpp::StoreQueryResult res = query_solution.store(ct_id);

			return (int) (res[0][0]);
		}

		void make_user_set(int problem_num)
		{
			mysqlpp::Query query_user = mysqlConn->query(
					"select u_id, u_name, u_realname "
					"from ctuser where ct_id = %0");
			query_user.parse();
			mysqlpp::StoreQueryResult user_res = query_user.store(this->ct_id);

			for (const auto & row : user_res) {
				this->user_set.emplace(row["u_id"], row["u_name"].c_str(), row["u_realname"].c_str(), problem_num);
			}
		}

		void make_solution()
		{
			using namespace std::chrono;

			auto now = system_clock::now();

			mysqlpp::Query query_solution = mysqlConn->query(
					"select u_id, p_logic, s_result, s_posttime "
					"from contest_solution%0 as so, contest_problem as cp "
					"where so.p_id = cp.p_id and ct_id = %1 and "
					"u_id in (select u_id from ctuser where ct_id = %1) order by s_id");
			query_solution.parse();
			mysqlpp::StoreQueryResult res = query_solution.store(this->ct_id, this->ct_id);

			for (const auto & row : res) {
				int u_id = row["u_id"];
				char p_logic = row["p_logic"].c_str()[0];
				if (('A' <= p_logic && p_logic <= 'Z') == false) {
					continue;
				}
				UnitedJudgeResult s_result = (UnitedJudgeResult)(int(row["s_result"]));
				auto s_posttime = mysqlpp_datetime_to_std_timepoint(row["s_posttime"]);

				auto duration_since_ct_start = duration_cast<seconds>(s_posttime - this->ct_starttime);

				bool is_submit_during_scoreboard_closed = false;
				if (now > this->ct_endtime) {
					is_submit_during_scoreboard_closed = false;
				} else {
					auto closed_scoreboard_time = this->ct_endtime - this->ct_lockrankbefore;
					is_submit_during_scoreboard_closed = closed_scoreboard_time < s_posttime && s_posttime < this->ct_endtime;
				}

				auto it = std::lower_bound(user_set.begin(), user_set.end(), u_id, [](const contest_user & user, int u_id) {
					return user.u_id < u_id;
				});
				if(it != user_set.end()) {
					it->add_solution(p_logic, s_result, duration_since_ct_start, is_submit_during_scoreboard_closed);
				}
			}
		}

	public:
		contest(int ct_id, std::unique_ptr<mysqlpp::Connection> && mysqlConn) :
				ct_id(ct_id), mysqlConn(std::move(mysqlConn)), problem_num(get_problem_num(ct_id))
		{
			make_user_set(this->problem_num);
			mysqlpp::Query query_contest_info = this->mysqlConn->query(
					"select ct_starttime, ct_endtime, ct_timeplus, ct_lockrankbefore "
					"from contest "
					"where ct_id = %0 "
			);
			query_contest_info.parse();
			mysqlpp::StoreQueryResult res = query_contest_info.store(this->ct_id);
			if (query_contest_info.errnum() != 0) {
				std::runtime_error e(query_contest_info.error());
				EXCEPT_FATAL(ct_id, 0, log_fp, "Error occurred while getting contest details.", e);
				throw e;
			}
			if (res.size() == 0) {
				std::runtime_error e("A contest id doesn't exist");
				EXCEPT_FATAL(ct_id, 0, log_fp, "Error occurred while getting contest details.", e);
				throw e;

			}
			this->ct_starttime = mysqlpp_datetime_to_std_timepoint(res[0]["ct_starttime"]);
			this->ct_endtime = mysqlpp_datetime_to_std_timepoint(res[0]["ct_endtime"]);
			if (res[0]["ct_timeplus"] == mysqlpp::null) {
				this->ct_timeplus = std::chrono::seconds(1200);
			} else {
				this->ct_timeplus = std::chrono::seconds((int) (res[0]["ct_timeplus"]));
			}
			if (res[0]["ct_lockrankbefore"] == mysqlpp::null) {
				this->ct_lockrankbefore = std::chrono::seconds(3600);
			} else {
				this->ct_lockrankbefore = std::chrono::seconds((int) (res[0]["ct_lockrankbefore"]));
			}
			make_solution();
		}

	private:
		static std::string get_users_scoreboard_details(const contest_user & user, int rank, std::chrono::seconds time_plus)
		{
			if (user.list.size() == 0) {
				return "[]";
			}
			std::string list = "";
			for (const user_problem & user_problem : user.list) {
				switch (user_problem.status) {
					case user_problem::solved: {
						boost::format templ_if_solved(R"------([0,%d,%d])------");
						templ_if_solved % user_problem.tried_times % user_problem.first_ac_time_since_ct_start.count();
						list += templ_if_solved.str();
						list += ',';
						break;
					}
					case user_problem::tried_incorrect_or_never_tried: {
						boost::format templ_if_incorrect(R"------([1,%d])------");
						templ_if_incorrect % user_problem.tried_times;
						list += templ_if_incorrect.str();
						list += ',';
						break;
					}
					case user_problem::tried_pending: {
						boost::format templ_if_pending(R"------([2,%d])------");
						templ_if_pending % user_problem.tried_times;
						list += templ_if_pending.str();
						list += ',';
						break;
					}
				}
			}
			list.pop_back(); // 去掉末尾的逗号
			boost::format row_templ(R"------([%d,%d,"%s","%s",%d,%d,[%s]])------");
			row_templ % rank % user.u_id % user.u_name % user.u_realname % user.get_solved_num() % user.get_total_time(time_plus).count() % list;

			return row_templ.str();
		}

	public:
		std::string get_json_score_board() const
		{
			if (user_set.size() == 0) {
				return "[]";
			}
			contest_user::sort_by_acm_rank_rules rules(this->ct_timeplus);
			std::multiset<contest_user, contest_user::sort_by_acm_rank_rules> user_rank(user_set.cbegin(), user_set.cend(), rules);
			std::string ranklist = "[";
			int i = 1;
			for (const auto & user : user_rank) {
				ranklist += get_users_scoreboard_details(user, i, this->ct_timeplus);
				ranklist += ',';
				++i;
			}
			ranklist.pop_back();
			ranklist += ']';
			return ranklist;
		}
};

int update_contest_scoreboard(int ct_id, kerbal::redis::RedisContext redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn)
{
	contest contest(ct_id, std::move(mysqlConn));

	kerbal::redis::Operation opt(redisConn);
	opt.set((boost::format("contest_scoreboard:%d") % ct_id).str(), contest.get_json_score_board());
	return 0;
}


