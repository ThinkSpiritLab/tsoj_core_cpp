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

	private:
		/**
		 * @brief 该方法实现了祖先类 UpdateJobBase 中规定的 update solution 表的接口, 将本次提交记录更新至每个竞赛对应的 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution() override final;

		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 * @warning 仅规定 update user and problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_and_problem() override final;

		/**
		 *
		 */
		bool this_problem_has_not_accepted();

		/**
		 * @brief 查询该 job 对应的 user 在 pid 问题下错误的次数
		 */
		int get_error_count();

		/**
		 * @brief 取得此用户此题在此次提交以前 AC, TO_DO 或 ATTEMPT 的状态
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual user_problem_status get_user_problem_status() override final;

		virtual void update_user_problem() override final;
		virtual ~ContestUpdateJob() = default;
};

#endif /* SRC_MASTER_CONTESTUPDATEJOB_HPP_ */
