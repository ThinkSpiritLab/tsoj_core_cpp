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

		// make_update_job 为一个全局函数，用于根据提供的信息生成一个具体的 UpdateJobBase 信息，
		// 而 UpdateJobBase的实例化应该被控制，将随意 new 一个出来的可能性暴露出来是不妥当的，
		// 因此构造函数定义为 protected，只能由 make_update_job 来生成，故 make_update_job 也需要成为友元函数。
		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	private:
		ExerciseUpdateJob(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual ~ExerciseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
