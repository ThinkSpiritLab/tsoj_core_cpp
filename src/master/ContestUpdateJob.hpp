/*
 * ContestUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_CONTESTUPDATEJOB_HPP_
#define SRC_MASTER_CONTESTUPDATEJOB_HPP_

#include "UpdateJobBase.hpp"

class ContestUpdateJob: public UpdateJobBase
{
	private:
		typedef UpdateJobBase supper_t;

		friend
		unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ContestUpdateJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
						 std::unique_ptr <mysqlpp::Connection> && mysqlConn);
		virtual void update_solution(const Result & result) override final;
		bool is_first_ac_solution();

		/**
		 * @brief 查询该 job 对应的 user 在 pid 问题下错误的次数
		 */
		int get_error_count();

		virtual void update_user_problem(int stat) override final;
		virtual ~ContestUpdateJob() = default;
};

#endif /* SRC_MASTER_CONTESTUPDATEJOB_HPP_ */
