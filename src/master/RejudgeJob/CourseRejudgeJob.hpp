/*
 * CourseRejudgeJob.hpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_COURSEREJUDGEJOB_HPP_
#define SRC_MASTER_REJUDGEJOB_COURSEREJUDGEJOB_HPP_

#include "ExerciseRejudgeJobBase.hpp"

class CourseRejudgeJob: public ExerciseRejudgeJobBase
{
	private:
		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	protected:
		CourseRejudgeJob(int jobType, ojv4::s_id_type s_id, ojv4::c_id_type c_id, kerbal::redis_v2::connection & redis_conn);

		ojv4::c_id_type c_id;

		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final;

	public:
		virtual ~CourseRejudgeJob() noexcept = default;
};



#endif /* SRC_MASTER_REJUDGEJOB_COURSEREJUDGEJOB_HPP_ */
