/*
 * RejudgeJobBase.hpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_
#define SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_

#include "UpdateJobBase.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/datetime.h>


class RejudgeJobBase: public UpdateJobBase
{
	private:

		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, ojv4::s_id_type s_id, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);

	protected:

		RejudgeJobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		mysqlpp::DateTime rejudge_time;

	public:
		virtual void handle() override final;

	private:
		virtual ojv4::s_result_enum move_orig_solution_to_rejudge_solution() = 0;

		virtual void update_solution() override = 0;

		virtual void update_source_code(const char *) override final
		{
		}

		virtual void update_compile_info(const char *) override final
		{
		}

		virtual void rejudge_compile_info(ojv4::s_result_enum orig_result) = 0;

		virtual void update_user() override = 0;

		virtual void update_problem() override = 0;

		virtual void update_user_problem() override = 0;

		virtual void update_user_problem_status() override = 0;

		virtual void send_rejudge_notification() = 0;

	public:
		virtual ~RejudgeJobBase() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_ */
