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
		make_update_job(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:
		ContestRejudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		ojv4::ct_id_type ct_id;

		virtual ojv4::s_result_enum move_orig_solution_to_rejudge_solution() override final;

		virtual void update_solution() override final;

		virtual void update_user() override final;

		virtual void update_problem() override final;

		virtual void update_user_problem() override final;

		virtual void update_user_problem_status() override final;

		virtual void rejudge_compile_info(ojv4::s_result_enum orig_result) override final;

		virtual void send_rejudge_notification() override final
        {
        }

	public:
		virtual ~ContestRejudgeJob() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_CONTESTREJUDGEJOB_HPP_ */
