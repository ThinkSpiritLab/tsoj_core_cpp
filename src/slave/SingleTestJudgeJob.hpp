/*
 * SingleTestJudgeJob.hpp
 *
 *  Created on: 2018年12月11日
 *      Author: peter
 */

#ifndef SRC_SLAVE_SINGLETESTJUDGEJOB_HPP_
#define SRC_SLAVE_SINGLETESTJUDGEJOB_HPP_

#include "JobInterface.hpp"
#include "db_typedef.hpp"
#include "LanguageStrategy.hpp"
#include "boost_format_suffix.hpp"

#include <memory>

#include <kerbal/redis_v2/connection.hpp>
#include <kerbal/redis_v2/reply.hpp>
#include <kerbal/data_struct/optional/optional.hpp>
#include <boost/filesystem.hpp>

class SingleTestJudgeJob: public JobInterface
{
		template <typename Type>
		using optional = kerbal::data_struct::optional<Type>;

		int jobType; ///< Job 类型，如：竞赛、课程等
		ojv4::s_id_type s_id; ///< solution id
		const boost::filesystem::path working_dir; ///< 本 job 的工作路径
		ojv4::p_id_type p_id; ///< problem id
		std::unique_ptr<LanguageStrategyInterface> languageStrategy;
		optional<ojv4::s_time_in_milliseconds> time_limit; ///< 时间限制
		optional<ojv4::s_mem_in_MB> memory_limit; ///< 空间限制
		optional<ojv4::s_similarity_type> similarity_threshold; ///< 重复率限制

	public:

		SingleTestJudgeJob(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

		virtual std::string description() const override
		{
			return "%d:%d"_fmt(jobType, s_id).str();
		}

		virtual void handle() override;

	private:
		virtual void store_source_code_procedure();

		virtual void compile_procedure(UnitedJudgeResult & judge_result);

		virtual void running_procedure(UnitedJudgeResult & judge_result, ProtectedProcessDetails & running_details);

		virtual void compare_procedure(UnitedJudgeResult & judge_result);

		virtual void commit_judge_result_to_redis(kerbal::redis_v2::connection & redis_conn,
													const UnitedJudgeResult & judge_result,
													const ojv4::s_time_in_milliseconds & cpu_time,
													const ojv4::s_time_in_milliseconds & real_time,
													const ojv4::s_mem_in_byte & memory,
													int similarity_percentage);

		virtual bool failed_handle(kerbal::redis_v2::connection & redis_conn) noexcept try
		{
			this->commit_judge_result_to_redis(
					redis_conn,
					UnitedJudgeResult::SYSTEM_ERROR,
					ojv4::s_time_in_milliseconds(),
					ojv4::s_time_in_milliseconds(),
					ojv4::s_mem_in_byte(),
					0);
			return true;
		} catch (...) {
			return false;
		}

		/**
		 * @brief 将当前路径切换出本 job 的临时工作路径并清理该工作区
		 * @exception 该函数保证不抛出任何异常
		 */
		bool clean_job_dir() noexcept;
};

class CompulsoryParameterMissedException: std::runtime_error
{
	public:
		CompulsoryParameterMissedException() :
				std::runtime_error("miss compulsory parameter in redis")
		{
		}
};

#endif /* SRC_SLAVE_SINGLETESTJUDGEJOB_HPP_ */
