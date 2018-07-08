/*
 * JobInfo.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_JOBINFO_HPP_
#define SRC_JOBINFO_HPP_

#include <kerbal/redis/redis_context.hpp>
#include <kerbal/utility/memory_storage.hpp>

#include <string>

#include "Config.hpp"
#include "Result.hpp"

using kerbal::redis::Context;

class JobInfo
{
	public:
		int jobType;
		int sid;
	protected:
		std::string dir;
	public:
		int pid;
		int uid;
		Language lang;
		int cases;
		int cid;
		std::chrono::milliseconds timeLimit;
		kerbal::utility::MB memoryLimit;
		std::string postTime;
		bool haveAccepted;
		bool no_store_ac_code;
		bool is_rejudge;

		static std::pair<int, int> parser_job_item(const std::string & job_item);
		static JobInfo fetchFromRedis(const Context & conn, int jobType, int sid);

		JobInfo(int jobType, int sid);
		void judge_job(const Context & conn);
		void change_job_dir() const;
		void clean_job_dir() const noexcept;
		void store_source_code(const Context & conn) const;
		Result execute(const Config & config) const noexcept;
		Result compile() const noexcept;
		void set_compile_info(const Context & conn);
		Result running(const Context & conn);
		int child_process(const Config & config) const;


		/**
		 * @brief Calculate this job's similarity
		 * @return similarity of this job
		 * @throw JobHandleException if any ERROR happened
		 * @author Chen Xi, Peter Nee
		 */
		int calculate_similarity() const;
		void store_code_to_accepted_solutions_dir() const;

		void push_back_failed_judge_job(const Context & conn) const noexcept;
		void commitJudgeStatusToRedis(const Context & conn, const std::string & key, JudgeStatus value);
		void commitJudgeResultToRedis(const Context & conn, const Result & result) const;

};

class JobHandleException: public std::exception
{
	protected:
		std::string reason;

	public:
		JobHandleException(const std::string & reason);
		virtual const char * what() const noexcept;
};


#endif /* SRC_JOBINFO_HPP_ */
