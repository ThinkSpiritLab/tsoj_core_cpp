/*
 * JobInfo.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_JOBINFO_HPP_
#define SRC_JOBINFO_HPP_

#include "JobBase.hpp"
#include "Result.hpp"

class Config;

class JudgeJob: public JobBase
{
	private:
		typedef JobBase supper_t;

	protected:
		std::string dir;

	public:
		static bool isExitJob(const std::string & jobItem)
		{
			return jobItem == "0,-1";
		}

		static std::string getExitJobItem()
		{
			return "0,-1";
		}

		JudgeJob(int jobType, int sid, const kerbal::redis::RedisContext & conn);
		void judge_job();
		void fetchDetailsFromRedis();

	protected:
		void change_job_dir() const;
		void clean_job_dir() const noexcept;
		Result execute(const Config & config) const noexcept;

		Result compile() const noexcept;
		bool set_compile_info() noexcept;
		Result running();
		bool child_process(const Config & config) const;

		void commitJudgeResultToRedis(const Result & result);

	public:
		void push_back_failed_judge_job() noexcept;
};


#endif /* SRC_JOBINFO_HPP_ */
