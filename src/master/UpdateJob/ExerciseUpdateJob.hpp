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

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn);

	private:
		ExerciseUpdateJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn);

		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual ~ExerciseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
