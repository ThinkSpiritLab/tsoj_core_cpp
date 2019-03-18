/*
 * RejudgeJobInterface.hpp
 *
 *  Created on: 2018年11月20日
 *	  Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_
#define SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_

#include "UpdateJobInterface.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/datetime.h>


class RejudgeJobInterface: protected ConcreteUpdateJob
{
	protected:
		mysqlpp::DateTime rejudge_time;

		RejudgeJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	private:
		virtual void handle() override final;

		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) = 0;

		virtual void send_rejudge_notification(mysqlpp::Connection & mysql_conn) = 0;

		virtual void clear_redis_info() noexcept;

	public:
		virtual ~RejudgeJobInterface() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_REJUDGEJOBBASE_HPP_ */
