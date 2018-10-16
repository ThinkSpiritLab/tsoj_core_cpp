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

using namespace std;

#include <boost/format.hpp>
using boost::format;

#include <kerbal/redis/operation.hpp>

#include "UpdateContestScoreboard.hpp"

class contest
{
	private:
		struct user_problem
		{
				int tried_times;
				std::chrono::seconds first_ac_time_since_ct_start;

				enum
				{
					solved, tried_incorrect_or_never_tried, tried_pending
				} status;

				user_problem() :
						tried_times(0), first_ac_time_since_ct_start(0), status(tried_incorrect_or_never_tried)
				{
				}
		};


		class contest_user
		{
			public:
				int u_id;
				std::string u_name;
				std::string u_realname;
				mutable int u_accept;
				mutable std::chrono::seconds total_time;
				mutable std::vector<user_problem> list;

				contest_user(int u_id, const std::string & u_name, const std::string & u_realname, int problem_num) :
						u_id(u_id), u_name(u_name), u_realname(u_realname), u_accept(0), total_time(0)
				{
					list.resize(problem_num);
				}

				int get_u_id() const
				{
					return this->u_id;
				}

				int get_u_accept() const
				{
					return this->u_accept;
				}

				std::chrono::seconds get_total_time() const
				{
					return this->total_time;
				}

				void add_solution(unsigned int logic_pid, int result, std::chrono::seconds posttime_since_ct_start, bool is_submit_during_scoreboard_closed, std::chrono::seconds time_plus) const
				{
					if (logic_pid >= list.size()) {
						return;
					}
					user_problem & user_problem = list[logic_pid];

					switch (user_problem.status) {
						case user_problem::solved:
							break;
						case user_problem::tried_incorrect_or_never_tried: {
							if (is_submit_during_scoreboard_closed == true) {
								user_problem.status = user_problem::tried_pending;
								++user_problem.tried_times;
							} else {
								if (result == 0) {
									user_problem.status = user_problem::solved;
									++this->u_accept;
									this->total_time += (user_problem.tried_times) * time_plus + posttime_since_ct_start;
									user_problem.first_ac_time_since_ct_start = posttime_since_ct_start;
								}
								++user_problem.tried_times;
							}
							break;
						}
						case user_problem::tried_pending: {
							++user_problem.tried_times;
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
						bool operator()(const contest_user & a, const contest_user & b) const
						{
							int a_ac_num = a.get_u_accept();
							int b_ac_num = b.get_u_accept();
							if (a_ac_num == b_ac_num) {
								return a.get_total_time() < b.get_total_time();
							}
							return a_ac_num > b_ac_num;
						}
				};

				static std::string format_seconds(std::chrono::seconds second)
				{
					int seconds = second.count();
					int minutes = seconds / 60;
					seconds %= 60;
					int hours = minutes / 60;
					minutes %= 60;

					auto to_2_di_string = [](int x) {
						if(x < 10) {
							std::string res = "00";
							res[1] = char(x + '0');
							return res;
						} else {
							return std::to_string(x);
						}
					};

					return to_2_di_string(hours) + ':' + to_2_di_string(minutes) + ':' + to_2_di_string(seconds);
				}

				std::string get_row(int rank) const
				{
					std::string list = "";
					for (const user_problem & user_problem : this->list) {
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
					list.pop_back();
					boost::format row_templ(
							R"------([%d,%d,"%s","%s",%d,%d,[%s]])------"
					);
					row_templ % rank % this->u_id % this->u_name % this->u_realname % this->get_u_accept() % this->get_total_time().count() % list;

					return row_templ.str();
				}
		};


		int ct_id;
		std::unique_ptr<mysqlpp::Connection> mysqlConn;
		int problem_num;
		std::chrono::system_clock::time_point ct_starttime;
		std::chrono::system_clock::time_point ct_endtime;
		std::chrono::seconds ct_timeplus;
		std::set<contest_user, contest_user::sort_by_u_id> user_set;

		static std::chrono::system_clock::time_point mysqlpp_datetime_to_std_timepoint(const mysqlpp::DateTime & src)
		{
			return std::chrono::system_clock::from_time_t((time_t)(src));
		}

		int get_problem_num(int cid)
		{
			mysqlpp::Query query_solution = mysqlConn->query(
					"select count(p_id) "
					"from contest_problem where ct_id = %0");
			query_solution.parse();
			mysqlpp::StoreQueryResult res = query_solution.store(cid);

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
				int s_result = row["s_result"];
				auto s_posttime = mysqlpp_datetime_to_std_timepoint(row["s_posttime"]);

				auto duration_since_ct_start = duration_cast<seconds>(s_posttime - this->ct_starttime);

				bool is_submit_during_scoreboard_closed = false;
				if (now > this->ct_endtime) {
					is_submit_during_scoreboard_closed = false;
				} else {
					auto closed_scoreboard_time = this->ct_endtime - hours(1);
					is_submit_during_scoreboard_closed = closed_scoreboard_time < s_posttime && s_posttime < this->ct_endtime;
				}

				auto it = std::lower_bound(user_set.begin(), user_set.end(), u_id, [](const contest_user & user, int u_id) {
					return user.get_u_id() < u_id;
				});
				it->add_solution((unsigned int)(p_logic - 'A'), s_result, duration_since_ct_start, is_submit_during_scoreboard_closed, this->ct_timeplus);
			}
		}

	public:
		contest(int ct_id, std::unique_ptr<mysqlpp::Connection> mysqlConn) : ct_id(ct_id), mysqlConn(std::move(mysqlConn)), problem_num(get_problem_num(ct_id))
		{
			make_user_set(this->problem_num);
			mysqlpp::Query query_contest_info = this->mysqlConn->query(
					"select ct_starttime, ct_endtime, ct_timeplus "
					"from contest "
					"where ct_id = %0 "
			);
			query_contest_info.parse();
			mysqlpp::StoreQueryResult res = query_contest_info.store(this->ct_id);
			if(query_contest_info.errnum() != 0) {
				cerr << query_contest_info.error() << endl;
				throw std::exception();
			}
			this->ct_starttime = mysqlpp_datetime_to_std_timepoint(res[0]["ct_starttime"]);
			this->ct_endtime = mysqlpp_datetime_to_std_timepoint(res[0]["ct_endtime"]);
			if (res[0]["ct_timeplus"] == mysqlpp::null) {
				this->ct_timeplus = std::chrono::seconds(1200);
			} else {
				this->ct_timeplus = std::chrono::seconds((int) (res[0]["ct_timeplus"]));
			}
			make_solution();
		}

		std::string get_json_score_board() const
		{
			std::multiset<contest_user, contest_user::sort_by_acm_rank_rules> user_rank(user_set.cbegin(), user_set.cend());
			std::string ranklist = "[";
			int i = 1;
			for (const auto & user : user_rank) {
				ranklist += user.get_row(i);
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


