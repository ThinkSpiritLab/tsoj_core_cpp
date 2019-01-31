/*
 * ExerciseRejudgeJobBase.hpp
 *
 *  Created on: 2018年11月20日
 *      Author: peter
 */

#ifndef SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOBBASE_HPP_
#define SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOBBASE_HPP_

#include "RejudgeJobBase.hpp"

class ExerciseRejudgeJobBase : public RejudgeJobBase
{
	protected:
		ExerciseRejudgeJobBase(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		virtual void move_orig_solution_to_rejudge_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_solution(mysqlpp::Connection & mysql_conn) override final;

		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

	public:
		virtual ~ExerciseRejudgeJobBase() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOBBASE_HPP_ */
