/*
 * CourseRejudgeUpdateJob.hpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEREJUDGEUPDATEJOB_HPP_
#define SRC_MASTER_COURSEREJUDGEUPDATEJOB_HPP_

#include "CourseUpdateJob.hpp"

class CourseRejudgeUpdateJob: public CourseUpdateJob
{
	private:

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		CourseRejudgeUpdateJob(int jobType, int sid, int cid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	private:
		void move_orig_solution_to_rejudge_solution();

		virtual void update_solution() override;

		virtual ~CourseRejudgeUpdateJob() noexcept = default;
};



#endif /* SRC_MASTER_COURSEREJUDGEUPDATEJOB_HPP_ */
