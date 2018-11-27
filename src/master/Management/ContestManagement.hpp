/*
 * ContestManagement.hpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#ifndef SRC_MASTER_CONTESTMANAGEMENT_HPP_
#define SRC_MASTER_CONTESTMANAGEMENT_HPP_

#include "db_typedef.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/connection.h>

#include <kerbal/redis/redis_context.hpp>

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

		static void update_scoreboard(mysqlpp::Connection & mysql_conn, kerbal::redis::RedisContext redis_conn, ojv4::ct_id_type ct_id);
};


#endif /* SRC_MASTER_CONTESTMANAGEMENT_HPP_ */
