/*
 * CourseManagement.hpp
 *
 *  Created on: 2018年11月11日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEMANAGEMENT_HPP_
#define SRC_MASTER_COURSEMANAGEMENT_HPP_

#include "db_typedef.hpp"

namespace mysqlpp
{
	class Connection;
}

class CourseManagement
{
	public:
		CourseManagement() = delete;

		/**
		 * @brief 更新编号为 c_id 的课程/考试中所有用户的提交数和通过数
		 * @warning 部分课程/考试由于结果敏感, 已禁止更新, 详见源码
		 */
		static void refresh_all_users_sa_num_in_course(
				mysqlpp::Connection & mysql_conn,
				oj::c_id_type c_id);

		static void refresh_all_problems_sa_num_in_course(
				mysqlpp::Connection & mysql_conn,
				oj::c_id_type c_id);

		static void update_user_s_sa_num(
				mysqlpp::Connection & mysql_conn,
				oj::c_id_type c_id,
				oj::u_id_type u_id);

		static void update_problem_s_sa_num(
				mysqlpp::Connection & mysql_conn,
				oj::c_id_type c_id,
				oj::p_id_type p_id);

		static void update_user_problem(
				mysqlpp::Connection & mysql_conn,
				oj::c_id_type c_id,
				oj::u_id_type u_id,
				oj::p_id_type p_id);
};

#endif /* SRC_MASTER_COURSEMANAGEMENT_HPP_ */
