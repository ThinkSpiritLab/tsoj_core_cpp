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
		ExerciseRejudgeJobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

		virtual ojv4::s_result_enum move_orig_solution_to_rejudge_solution() override final;

		virtual void update_solution() override final;

		virtual void rejudge_compile_info(ojv4::s_result_enum orig_result) override final;

	public:
		virtual ~ExerciseRejudgeJobBase() noexcept = default;
};

#endif /* SRC_MASTER_REJUDGEJOB_EXERCISEREJUDGEJOBBASE_HPP_ */
