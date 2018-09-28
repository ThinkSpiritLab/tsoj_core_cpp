/*
 * ExerciseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOB_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOB_HPP_

#include "ExerciseUpdateJobBase.hpp"

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

		virtual void update_user_and_problem() override final;

		virtual void update_user_problem() override final;

		virtual ~ExerciseUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
