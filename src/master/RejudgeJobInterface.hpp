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
	protected:
		RejudgeJobBase(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		mysqlpp::DateTime rejudge_time;

	public:
		virtual void handle() override final;

	private:
		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) = 0;

		virtual void update_solution(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void update_source_code(mysqlpp::Connection & mysql_conn) override final
		{
		}

		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void update_user(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void update_problem(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void update_user_problem_status(mysqlpp::Connection & mysql_conn) override = 0;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) = 0;

		virtual void clear_redis_info() noexcept;

	public:
		virtual ~RejudgeJobBase() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_ */
