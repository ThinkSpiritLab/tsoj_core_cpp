/*
 * JobInfo.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_JOBINFO_HPP_
#define SRC_JOBINFO_HPP_

#include <kerbal/utility/memory_storage.hpp>
#include <string>
#include "Result.hpp"

class Config;

namespace kerbal
{
	namespace redis
	{
		class Context;
	}
}

class JobInfo
{
	public:
		int jobType;
		int sid;
	protected:
		std::string dir;
	public:
		int pid; ///@brief problem id
		int uid; ///@brief user id
		Language lang; ///@brief language
		int cases; ///@brief how many test cases the problem have
		int cid;
		std::chrono::milliseconds timeLimit; ///@brief time limit of this problem
		kerbal::utility::MB memoryLimit; ///@brief memory limit of this problem
		std::string postTime; ///@brief post time
		bool haveAccepted; ///@brief whether the user has pass the problem before
		bool no_store_ac_code;
		bool is_rejudge; ///@brief is rejudge

		static std::pair<int, int> parser_job_item(const std::string & job_item);
		static JobInfo fetchFromRedis(const kerbal::redis::Context & conn, int jobType, int sid);

		JobInfo(int jobType, int sid);
		void judge_job(const kerbal::redis::Context & conn);
		void change_job_dir() const;
		void clean_job_dir() const noexcept;
		void store_source_code(const kerbal::redis::Context & conn) const;
		Result execute(const Config & config) const noexcept;


		Result compile() const noexcept;
		bool set_compile_info(const kerbal::redis::Context & conn) noexcept;
		Result running(const kerbal::redis::Context & conn);
		int child_process(const Config & config) const;


		/**
		 * @brief Calculate this job's similarity
		 * @return similarity of this job
		 * @throw JobHandleException if any ERROR happened
		 */
		int calculate_similarity() const;
		void store_code_to_accepted_solutions_dir() const;

		void push_back_failed_judge_job(const kerbal::redis::Context & conn) const noexcept;
		void commitJudgeStatusToRedis(const kerbal::redis::Context & conn, JudgeStatus value);
		void commitJudgeResultToRedis(const kerbal::redis::Context & conn, const Result & result) const;

};

class JobHandleException: public std::exception
{
	protected:
		std::string reason;

	public:
		JobHandleException(const std::string & reason) :
				reason(reason)
		{
		}

		virtual const char * what() const noexcept
		{
			return reason.c_str();
		}
};

#endif /* SRC_JOBINFO_HPP_ */
