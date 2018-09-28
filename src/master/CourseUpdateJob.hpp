/*
 * CourseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEUPDATEJOB_HPP_
#define SRC_MASTER_COURSEUPDATEJOB_HPP_

#include "ExerciseUpdateJobBase.hpp"

class CourseUpdateJob: public ExerciseUpdateJobBase
{
	private:
		typedef ExerciseUpdateJobBase supper_t;

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		CourseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);
	private:
		virtual void update_solution() override final;

		/**
		 * @brief 将本 job 本课程对应的 pid 的题目的提交数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_course_problem_submit(int delta);

		/**
		 * @brief 将本 job 本课程对应的 pid 的题目的通过数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_course_problem_accept(int delta);

		/**
		 * @brief 将本 job 本课程对应的 uid 的题目的提交数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_course_user_submit(int delta);

		/**
		 * @brief 将本 job 本课程对应的 uid 的题目的通过数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_course_user_accept(int delta);

		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 * @warning 仅规定 update user and problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_and_problem() override final;

		user_problem_status get_course_user_problem_status();

		virtual void update_user_problem() override final;

		virtual ~CourseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_COURSEUPDATEJOB_HPP_ */
