 /*
 * CourseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEUPDATEJOB_HPP_
#define SRC_MASTER_COURSEUPDATEJOB_HPP_

#include "ExerciseUpdateJobBase.hpp"

/**
 * @brief 课程练习类，定义了其独特的 update_solution 等操作。针对课程，此类还维护 user_problem 表中 cid 非空的数据。
 * 该数据与普通的 user_problem 记录独立。即同一个 u_id 与 p_id 的数据也根据 cid 不同而不同。竞赛中用户的提交信息也记录
 * 到 course_user 表中，在普通练习未 AC 的情况下，课程中的提交与 AC 会相应增加或更新到 user 表中。该 course_user 表也由此类维护。
 */
class CourseUpdateJob: public ExerciseUpdateJobBase
{
	private:

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	protected:
		ojv4::c_id_type c_id; ///< @brief course_id

		CourseUpdateJob(int jobType, ojv4::s_id_type s_id, ojv4::c_id_type c_id, kerbal::redis_v2::connection & redis_conn);

	private:
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override;

		/**
		 * @brief 更新用户的提交数, 通过数
		 * @warning 仅规定 update user 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 更新题目的提交数, 通过数
		 * @warning 仅规定 update roblem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual ~CourseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_COURSEUPDATEJOB_HPP_ */
