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
		make_update_job(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ExerciseRejudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		virtual void update_user() override final;

		virtual void update_problem() override final;

		virtual void update_user_problem() override final;

		virtual void update_user_problem_status() override final;

		virtual void send_rejudge_notification() override final;

	public:
		virtual ~ExerciseRejudgeJob() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOB_HPP_ */
