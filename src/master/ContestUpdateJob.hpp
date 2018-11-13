/*
 * ContestUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_CONTESTUPDATEJOB_HPP_
#define SRC_MASTER_CONTESTUPDATEJOB_HPP_

#include "UpdateJobBase.hpp"

/**
 * @brief 竞赛类，定义了其独特的 update_solution 操作以及竞赛所需的统计非 AC 提交数等功能
 * 同时，该类负责维护所有 contest 相关的 source, compile_info, solution, user_problem 等表
 */
class ContestUpdateJob: public UpdateJobBase
{
	private:
		typedef UpdateJobBase supper_t;

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

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
		 * @brief 将提交代码更新至 mysql
		 * @param source_code 指向代码字符串的常量指针
		 */
		virtual void update_source_code(const char * source_code) override final;

		/**
		 * @brief 将编译错误信息更新至 mysql
		 * @param compile_info 指向编译错误信息字符串的常量指针
		 */
		virtual void update_compile_info(const char * compile_info) override final;

		/**
		 * @brief 更新用户的提交数, 通过数
		 * @warning 仅规定 update user 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user() override final;

		/**
		 * @brief 更新题目的提交数, 通过数
		 * @warning 仅规定 update problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_problem() override final;

		virtual void update_user_problem() override final;

		virtual void update_user_problem_status() override final;

		virtual ~ContestUpdateJob() noexcept = default;
};

#endif /* SRC_MASTER_CONTESTUPDATEJOB_HPP_ */
