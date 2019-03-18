/*
 * ExerciseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOB_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOB_HPP_

#include "BasicUpdateJobInterface.hpp"
#include "RejudgeJobInterface.hpp"

/**
 * @brief 普通练习类，定义了其独特的 update_solution 操作，即增加特有的 solution 记录。
 * 而维护的 user 表、user_problem 表与 problem 表，均使用基类的定义完成。
 */
class ExerciseJobInterface: virtual protected UpdateJobInterface
{
	protected:
		ExerciseJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		/**
		 * @brief 更新用户的提交数, 通过数
		 */
		virtual void update_user(mysqlpp::Connection & mysql_conn) override;

		/**
		 * @brief 更新题目的提交数, 通过数
		 */
		virtual void update_problem(mysqlpp::Connection & mysql_conn) override;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override;
};

/**
 * @brief ExerciseUpdateJob 与 CourseUpdateJob 的基类，定义了练习和课程中的通用部分。
 */
class BasicExerciseJobView : protected BasicJobInterface
{
	protected:
		BasicExerciseJobView(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	private:
		/**
		 * @brief 将提交代码更新至 mysql
		 */
		virtual void update_source_code(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 将编译错误信息更新至 mysql
		 */
		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;
};


class BasicExerciseJob : private BasicExerciseJobView, private ExerciseJobInterface
{
	private:

		// make_update_job 为一个全局函数，用于根据提供的信息生成一个具体的 UpdateJobInterface 信息，
		// 而 UpdateJobBase的实例化应该被控制，将随意 new 一个出来的可能性暴露出来是不妥当的，
		// 因此构造函数定义为 protected，只能由 make_update_job 来生成，故 make_update_job 也需要成为友元函数。
		friend
		std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		BasicExerciseJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		/**
		 * @brief 该方法实现了祖先类 UpdateJobInterface 中规定的 update solution 表的接口, 将本次提交记录更新至每个竞赛对应的 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

};

class RejudgeExerciseJobView : protected RejudgeJobInterface
{
	protected:
		RejudgeExerciseJobView(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	private:
		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 该方法实现了祖先类 UpdateJobInterface 中规定的 update solution 表的接口, 将本次提交记录更新至每个竞赛对应的 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

};

class RejudgeExerciseJob : private RejudgeExerciseJobView, private ExerciseJobInterface
{
	private:
		friend std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		RejudgeExerciseJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final;

};


#endif /* SRC_MASTER_EXERCISEUPDATEJOB_HPP_ */
