/*
 * ExerciseUpdateJobBase.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_

#include "UpdateJobBase.hpp"

class ExerciseUpdateJobBase : public UpdateJobBase
{
	private:
		typedef UpdateJobBase supper_t;

		friend
		unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ExerciseUpdateJobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		/**
		 * @brief 将本 job 对应的 pid 的题目的提交数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_problem_submit(int delta);

		/**
		 * @brief 将本 job 对应的 pid 的题目的通过数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_problem_accept(int delta);

		/**
		 * @brief 将本 job 对应的 uid 的题目的提交数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_user_submit(int delta);

		/**
		 * @brief 将本 job 对应的 uid 的题目的通过数 + delta
		 * @warning 本函数仅完成字段的更新操作, 不会检查操作的合法性!
		 */
		void update_user_accept(int delta);

		/**
		 * @brief 查询本 job 对应的 user 的 pid 题之前是否 AC 过
		 * @return AC 过返回 true, 否则返回 false
		 * @warning 本函数为纯虚函数, 具体实现依赖于子类
		 */
		virtual bool already_AC_before() = 0;

		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 */
		virtual void update_user_and_problem(const Result & result);

		virtual ~ExerciseUpdateJobBase() = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_ */
