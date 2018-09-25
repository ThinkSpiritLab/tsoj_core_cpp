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
		unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		CourseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);
	private:
		void update_solution() override final;

		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 * @warning 仅规定 update user and problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_and_problem() override;

		virtual user_problem_status get_user_problem_status() override final;

		virtual void update_user_problem() override final;

		virtual ~CourseUpdateJob() = default;
};

#endif /* SRC_MASTER_COURSEUPDATEJOB_HPP_ */
