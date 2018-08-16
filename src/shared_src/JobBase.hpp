/*
 * JobBase.hpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#ifndef SRC_JOBBASE_HPP_
#define SRC_JOBBASE_HPP_

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_context.hpp>
#include <kerbal/utility/memory_storage.hpp>

#include <string>
#include <chrono>

#include "united_resource.hpp"

class JobBase
{
	public:
		int jobType;
		int sid;
		kerbal::redis::RedisContext redisConn;

	public:
		int pid; ///@brief problem id
		Language lang; ///@brief language
		int cases; ///@brief how many test cases the problem have
		std::chrono::milliseconds timeLimit; ///@brief time limit of this problem
		kerbal::utility::MB memoryLimit; ///@brief memory limit of this problem

		static std::pair<int, int> parserJobItem(const std::string & jobItem);
		JobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn);
		virtual ~JobBase() = default;
		virtual void fetchDetailsFromRedis() = 0;
		void storeSourceCode() const;
		void commitJudgeStatusToRedis(JudgeStatus value);
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


#endif /* SRC_JOBBASE_HPP_ */
