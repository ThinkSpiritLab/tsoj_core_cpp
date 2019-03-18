/*
 * ExerciseManagement.hpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEMANAGEMENT_HPP_
#define SRC_MASTER_EXERCISEMANAGEMENT_HPP_

#include "db_typedef.hpp"

namespace mysqlpp
{
	class Connection;
}

class ExerciseManagement
{
	public:
		ExerciseManagement() = delete;

		static void refresh_all_users_sa_num(mysqlpp::Connection & mysql_conn);

		static void refresh_all_problems_sa_num(mysqlpp::Connection & mysql_conn);

		static void refresh_all_user_problem(mysqlpp::Connection & mysql_conn);

		static void update_user_s_sa_num(mysqlpp::Connection & mysql_conn, oj::u_id_type u_id);

		static void update_problem_s_sa_num(mysqlpp::Connection & mysql_conn, oj::p_id_type p_id);

		static void update_user_problem(mysqlpp::Connection & mysql_conn, oj::u_id_type u_id, oj::p_id_type p_id);
};

#endif /* SRC_MASTER_EXERCISEMANAGEMENT_HPP_ */
