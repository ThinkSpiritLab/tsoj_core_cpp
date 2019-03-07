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

#include <kerbal/redis_v2/connection.hpp>

class ContestManagement
{
	public:
		ContestManagement() = delete;

		static void refresh_all_problems_submit_and_accept_num_in_contest(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id);

		static void update_problem_s_submit_and_accept_num(
				mysqlpp::Connection & mysql_conn,
				ojv4::ct_id_type ct_id,
				ojv4::p_id_type p_id);

		static void update_user_problem(
				mysqlpp::Connection & mysql_conn,
				ojv4::ct_id_type ct_id,
				ojv4::u_id_type u_id,
				ojv4::p_id_type p_id);

		static void update_user_problem_status(
				mysqlpp::Connection & mysql_conn,
				ojv4::ct_id_type ct_id,
				ojv4::u_id_type u_id,
				ojv4::p_id_type p_id);

		static void update_scoreboard(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				ojv4::ct_id_type ct_id);

		static void update_scoreboard2(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				ojv4::ct_id_type ct_id);

		static void update_scoreboard3(
				mysqlpp::Connection & mysql_conn,
				kerbal::redis_v2::connection & redis_conn,
				ojv4::ct_id_type ct_id);
};


#endif /* SRC_MASTER_CONTESTMANAGEMENT_HPP_ */
