/*
 * ExerciseRejudgeJob.hpp
 *
 *  Created on: 2018年11月20日
 *      Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOB_HPP_
#define SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOB_HPP_

#include "ExerciseRejudgeJobBase.hpp"

class ExerciseRejudgeJob : public ExerciseRejudgeJobBase
{
	private:

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn);

	protected:
		ExerciseRejudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn);

		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final;

	public:
		virtual ~ExerciseRejudgeJob() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOB_HPP_ */
