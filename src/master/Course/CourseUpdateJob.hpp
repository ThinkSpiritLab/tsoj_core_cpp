 /*
 * CourseUpdateJob.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_COURSEUPDATEJOB_HPP_
#define SRC_MASTER_COURSEUPDATEJOB_HPP_

#include "ExerciseUpdateJob.hpp"

/**
 * @brief 课程练习类，定义了其独特的 update_solution 等操作。针对课程，此类还维护 user_problem 表中 cid 非空的数据。
 * 该数据与普通的 user_problem 记录独立。即同一个 u_id 与 p_id 的数据也根据 cid 不同而不同。竞赛中用户的提交信息也记录
 * 到 course_user 表中，在普通练习未 AC 的情况下，课程中的提交与 AC 会相应增加或更新到 user 表中。该 course_user 表也由此类维护。
 */
class CourseJobInterface: protected ExerciseJobInterface
{
	protected:
		oj::c_id_type c_id; ///< @brief course_id

		CourseJobInterface(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn);

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

	public:
		virtual ~CourseJobInterface() noexcept = default;
};

class BasicCourseJob: private BasicExerciseJobView, private CourseJobInterface
{
	private:

		// make_update_job 为一个全局函数，用于根据提供的信息生成一个具体的 UpdateJobInterface 信息，
		// 而 UpdateJobBase的实例化应该被控制，将随意 new 一个出来的可能性暴露出来是不妥当的，
		// 因此构造函数定义为 protected，只能由 make_update_job 来生成，故 make_update_job 也需要成为友元函数。
		friend
		std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		BasicCourseJob(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn);

		/**
		 * @brief 该方法实现了祖先类中规定的 update solution 表的接口, 将本次提交记录更新至 solution 表
		 * @warning 该方法已被标记为 final, 禁止子类覆盖本方法
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

	public:
		virtual ~BasicCourseJob() noexcept = default;
};

class RejudgeCourseJob: private RejudgeExerciseJobView, private CourseJobInterface
{
	private:
		friend
		std::unique_ptr<ConcreteUpdateJob>
		make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);


		RejudgeCourseJob(int jobType, oj::s_id_type s_id, oj::c_id_type c_id, kerbal::redis_v2::connection & redis_conn);

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final;

	public:
		virtual ~RejudgeCourseJob() noexcept = default;
};

#endif /* SRC_MASTER_COURSEUPDATEJOB_HPP_ */
