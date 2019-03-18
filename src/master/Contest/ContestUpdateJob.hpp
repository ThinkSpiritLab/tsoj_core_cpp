/*
 * ContestUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_CONTESTUPDATEJOB_HPP_
#define SRC_MASTER_CONTESTUPDATEJOB_HPP_

#include "BasicUpdateJobInterface.hpp"
#include "RejudgeJobInterface.hpp"

/**
 * @brief 竞赛类，定义了其独特的 update_solution 操作以及竞赛所需的统计非 AC 提交数等功能
 * 同时，该类负责维护所有 contest 相关的 source, compile_info, solution, user_problem 等表
 */
class ContestUpdateJobInterface: virtual private UpdateJobInterface
{
	protected:
		oj::ct_id_type ct_id;

		ContestUpdateJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	private:
		/**
		 * @brief 更新用户的提交数, 通过数
		 */
		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 更新题目的提交数, 通过数
		 */
		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;
};


class BasicContestJob : private BasicJobInterface, private ContestUpdateJobInterface
{
	private:
		friend
		std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		BasicContestJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		/**
		 * @brief 该方法实现了祖先类 UpdateJobInterface 中规定的 update solution 表的接口, 将本次提交记录更新至每个竞赛对应的 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 将提交代码更新至 mysql
		 */
		virtual void update_source_code(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 将编译错误信息更新至 mysql
		 */
		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

};

class RejudgeContestJob : private RejudgeJobInterface, private ContestUpdateJobInterface
{
	private:
		friend std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		RejudgeContestJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 该方法实现了祖先类 UpdateJobInterface 中规定的 update solution 表的接口, 将本次提交记录更新至每个竞赛对应的 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final
		{
		}

};


#endif /* SRC_MASTER_CONTESTUPDATEJOB_HPP_ */
