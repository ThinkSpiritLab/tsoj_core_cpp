/*
 * ExerciseUpdateJobBase.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_

#include "UpdateJobBase.hpp"

/**
 * @brief ExerciseUpdateJob 与 CourseUpdateJob 的虚基类，定义了解题状态查询与更新提交通过数等通用部分。
 */
class ExerciseUpdateJobBase : public UpdateJobBase
{
	private:
		typedef UpdateJobBase supper_t;

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ExerciseUpdateJobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
								std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		/**
		 * @brief 将本次提交记录更新至 solution 表
		 * @warning 仅规定 update solution 表的接口, 具体操作需由子类实现
		 */
		virtual void update_solution() override = 0;

	private:
		/**
		 * @brief 将提交代码更新至 mysql
		 * @param source_code 指向代码字符串的常量指针
		 */
		virtual void update_source_code(const char * source_code) override final;

		/**
		 * @brief 将编译错误信息更新至 mysql
		 * @param compile_info 指向编译错误信息字符串的常量指针
		 */
		virtual void update_compile_info(const char * compile_info) override final;

	protected:
		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 * @warning 仅规定 update user and problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_and_problem() override = 0;

	private:
		/**
		 * @brief 更新本 job 对应的 pid 的题目的提交数和通过数
		 */
		void update_problem_submit_and_accept_num_in_exercise();

		/**
		 * @brief 更新本 job 对应的 uid 的用户的提交数和通过数
		 */
		void update_user_submit_and_accept_num_in_exercise();

		/**
		 * @brief 取得此用户此题在此次提交以前 AC, TO_DO 或 ATTEMPT 的状态
		 * @warning 虽给了默认的实现, 但仍是纯虚函数, 需由子类实现具体操作
		 */
		user_problem_status get_exercise_user_problem_status();

	protected:

		virtual void update_user_problem_status() override = 0;

		virtual ~ExerciseUpdateJobBase() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_ */