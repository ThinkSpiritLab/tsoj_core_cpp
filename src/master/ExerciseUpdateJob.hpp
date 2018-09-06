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
		void update_solution(const Result & result) override final;
		virtual void update_user_problem(int stat) override final;

		/**
		 * @brief 查询本 job 对应的 user 的 pid 题之前是否 AC 过
		 * @return AC 过返回 true, 否则返回 false
		 * @warning 本函数完成了父类中定义的纯虚函数的实现, 并且禁止其子类覆盖此方法
		 */
		virtual bool already_AC_before() override final;
		virtual ~ExerciseUpdateJob() = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
