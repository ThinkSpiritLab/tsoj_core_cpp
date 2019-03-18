/*
 * ContestManagement.hpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#ifndef SRC_MASTER_CONTESTMANAGEMENT_HPP_
#define SRC_MASTER_CONTESTMANAGEMENT_HPP_

#include "db_typedef.hpp"

namespace mysqlpp
{
	class Connection;
}

namespace kerbal
{
	namespace redis_v2
	{
		class connection;
	}
}

class ContestManagement
{
	public:
		ContestManagement() = delete;

		static void refresh_all_problems_sa_num_in_contest(
				mysqlpp::Connection & mysql_conn,
				oj::ct_id_type ct_id);

		static void update_problem_s_sa_num(
				mysqlpp::Connection & mysql_conn,
				oj::ct_id_type ct_id,
				oj::p_id_type p_id);

		static void update_user_problem(
				mysqlpp::Connection & mysql_conn,
				oj::ct_id_type ct_id,
				oj::u_id_type u_id,
				oj::p_id_type p_id);

		static void update_scoreboard(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				oj::ct_id_type ct_id);

		static void update_scoreboard2(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				oj::ct_id_type ct_id);

		static void update_scoreboard3(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				oj::ct_id_type ct_id);

		static void update_scoreboard4(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				oj::ct_id_type ct_id);
};


#endif /* SRC_MASTER_CONTESTMANAGEMENT_HPP_ */
