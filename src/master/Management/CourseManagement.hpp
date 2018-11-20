/*
 * CourseManagement.hpp
 *
 *  Created on: 2018年11月11日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEMANAGEMENT_HPP_
#define SRC_MASTER_COURSEMANAGEMENT_HPP_

#include "ojv4_db_type.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/connection.h>


class CourseManagement
{
	public:
		CourseManagement() = delete;

		/**
		 * @brief 更新编号为 c_id 的课程/考试中所有用户的提交数和通过数
		 * @warning 部分课程/考试由于结果敏感, 已禁止更新, 详见源码
		 */
		static void refresh_all_users_submit_and_accept_num_in_course(ojv4::c_id_type c_id);

		static void refresh_all_problems_submit_and_accept_num_in_course(ojv4::c_id_type c_id);

		static void update_user_s_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id);

		static void update_problem_s_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id, ojv4::p_id_type p_id);

		static void update_user_problem(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id);

		static void update_user_problem_status(mysqlpp::Connection & mysql_conn, ojv4::c_id_type c_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id);

};

#endif /* SRC_MASTER_COURSEMANAGEMENT_HPP_ */
