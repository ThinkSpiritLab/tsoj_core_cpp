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
		void update_solution() override final;
		virtual void update_user_problem(int stat) override final;
		virtual ~CourseUpdateJob() = default;
};

#endif /* SRC_MASTER_COURSEUPDATEJOB_HPP_ */
