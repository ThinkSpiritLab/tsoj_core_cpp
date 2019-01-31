/*
 * ContestRejudgeJob.hpp
 *
 *  Created on: 2018年11月20日
 *      Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_CONTESTREJUDGEJOB_HPP_
#define SRC_MASTER_REJUDGEJOB_CONTESTREJUDGEJOB_HPP_

#include "RejudgeJobBase.hpp"

class ContestRejudgeJob : public RejudgeJobBase
{
	private:

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	protected:
		ContestRejudgeJob(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		ojv4::ct_id_type ct_id;

		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) override final
        {
        }

	public:
		virtual ~ContestRejudgeJob() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_CONTESTREJUDGEJOB_HPP_ */
