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
		unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ExerciseUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);
		void update_solution() override final;
		virtual void update_user_problem() override final;

		virtual ~ExerciseUpdateJob() = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
