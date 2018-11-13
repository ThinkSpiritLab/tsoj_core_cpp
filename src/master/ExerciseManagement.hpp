/*
 * ExerciseManagement.hpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEMANAGEMENT_HPP_
#define SRC_MASTER_EXERCISEMANAGEMENT_HPP_

#include "ojv4_db_type.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>


class ExerciseManagement
{
	public:
		ExerciseManagement() = delete;

		static void refresh_all_users_submit_and_accept_num();

		static void refresh_all_problems_submit_and_accept_num();

		static void update_user_s_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::u_id_type u_id);

		static void update_problem_s_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::p_id_type p_id);

		static void update_user_problem(mysqlpp::Connection & mysql_conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id);

		static void update_user_problem_status(mysqlpp::Connection & mysql_conn, ojv4::u_id_type u_id, ojv4::p_id_type p_id);

};

#endif /* SRC_MASTER_EXERCISEMANAGEMENT_HPP_ */
