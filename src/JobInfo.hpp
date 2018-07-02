/*
 * JobInfo.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_JOBINFO_HPP_
#define SRC_JOBINFO_HPP_

#include <kerbal/redis/redis_type_cast.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>
using namespace kerbal::redis;

#include <kerbal/utility/memory_storage.hpp>

#include <string>

#include "Config.hpp"
#include "judger.hpp"


class JobInfo
{
	protected:
		static boost::format key_name_tmpl;
		static boost::format judge_status_name_tmpl;

	public:
		int jobType;
		int sid;
	protected:
		std::string key_name;
		std::string judge_status_name;
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

		static std::pair<int, int> parser_job_item(std::string job_item);

		JobInfo(int jobType, int sid);
		static JobInfo fetchFromRedis(const Context & conn, int jobType, int sid);
		void judge_job(const Context & conn);
		Result running(const Context & conn);

		void commitJudgeStatusToRedis(const Context & conn, const std::string & key, JudgeStatus value);
		void commitJudgeResultToRedis(const Context & conn, const Result & result) const;

		/**
		 * @brief Calculate this job's similarity
		 * @return similarity of this job
		 * @throw CalculateSimilarityException if any ERROR happened
		 * @author Chen Xi, Peter Nee
		 */
		int calculate_similarity() const;
		int store_code_to_accepted_solutions_dir() const;

		void push_back_failed_judge_job(const Context & conn, List<std::string> & judge_failer_list) const;

		void change_job_dir() ;
		void clean_job_dir() const;
		Result run(const Config & config) const noexcept;
		Result compile() const;
		void set_compile_info(const Context & conn, const char *compile_info);
		int get_compile_info(const Context & conn);

		int child_process(const Config & _config) const;

		void store_source_code(const Context & conn) const;
};

class CalculateSimilarityException: public std::exception
{
	protected:
		std::string reason;

	public:
		CalculateSimilarityException(const std::string & reason) :
				reason(reason)
		{
		}

		virtual const char * what() const noexcept
		{
			return reason.c_str();
		}
};


#endif /* SRC_JOBINFO_HPP_ */
