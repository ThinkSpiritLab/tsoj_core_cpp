/*
 * ExerciseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOB_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOB_HPP_

#include "ExerciseUpdateJobBase.hpp"

/**
 * @brief 普通练习类，定义了其独特的 update_solution 操作，即增加特有的 solution 记录。
 * 而维护的 user 表、user_problem 表与 problem 表，均使用基类的定义完成。
 */
class ExerciseUpdateJob: public ExerciseUpdateJobBase
{
	private:
		typedef ExerciseUpdateJobBase supper_t;

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	private:
		ExerciseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);
		virtual void update_solution() override final;

		virtual void update_user() override final;

		virtual void update_problem() override final;

		virtual void update_user_problem_status() override final;

		virtual ~ExerciseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
