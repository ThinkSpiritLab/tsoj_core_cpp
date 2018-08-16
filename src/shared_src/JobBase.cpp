/*
 * JobBase.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */


#include "JobBase.hpp"
#include "logger.hpp"

#include <fstream>
#include <algorithm>

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_type_cast.hpp>

#include <boost/format.hpp>

using namespace kerbal::redis;

extern std::ostream log_fp;

std::pair<int, int> JobBase::parserJobItem(const std::string & args)
{
	std::string job_item = args;
	std::string::size_type cut_point = job_item.find(',');

	if (cut_point == std::string::npos) {
		LOG_FATAL(0, 0, log_fp, "Invalid job_item arguments: ", job_item);
		throw std::invalid_argument("Invalid job_item arguments: " + job_item);
	}

	job_item[cut_point] = '\0';

	try {
		int job_type = std::stoi(job_item.c_str());
		int job_id = std::stoi(job_item.c_str() + cut_point + 1);
		return std::make_pair(job_type, job_id);
	} catch (const std::invalid_argument & e) {
		LOG_FATAL(0, 0, log_fp, "Invalid job_item arguments: ", job_item);
		throw std::invalid_argument("Invalid job_item arguments: " + job_item);
	}
}

void JobBase::fetchDetailsFromRedis()
{
	using std::stoi;
	static boost::format key_name_tmpl("job_info:%d:%d");

	std::vector<std::string> query_res;

	try {
		Operation opt(redisConn);
		query_res = opt.hmget((key_name_tmpl % jobType % sid).str(), "pid", "lang", "cases", "time_limit", "memory_limit");
	} catch (const RedisUnexpectedCaseException & e) {
		LOG_FATAL(0, sid, log_fp, "redis returns an unexpected type. Exception infomation: ", e.what());
		throw;
	} catch (const RedisException & e) {
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
		throw;
	} catch (const std::exception & e) {
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
		throw;
	} catch (...) {
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", "unknown exception");
		throw;
	}

	try {
		this->pid = stoi(query_res[0]);
		this->lang = (Language) stoi(query_res[1]);
		this->cases = stoi(query_res[2]);
		this->timeLimit = std::chrono::milliseconds(stoi(query_res[3]));
		this->memoryLimit = kerbal::utility::MB(stoull(query_res[4]));
	} catch (const std::exception & e) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", e.what());
		throw;
	} catch (...) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", "unknown exception");
		throw;
	}
}

JobBase::JobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn) :
		jobType(jobType), sid(sid), redisConn(redisConn)
{
}

void JobBase::storeSourceCode() const
{
	static RedisCommand cmd("hget source_code:%%d:%%d source");
	RedisReply reply;
	try {
		reply = cmd.execute(redisConn, jobType, sid);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Get source code failed. Error information: ", e.what());
		throw JobHandleException("Get source code failed");
	}

	static const std::string stored_file_name[] = { "Main.c", "Main.cpp", "Main.java" };
	size_t i = 1;
	switch (lang) {
		case Language::Cpp:
		case Language::Cpp14:
			i = 1;
			break;
		case Language::C:
			i = 0;
			break;
		case Language::Java:
			i = 2;
			break;
	}
	std::ofstream fout(stored_file_name[i], std::ios::out);
	if (!fout) {
		LOG_FATAL(jobType, sid, log_fp, "store source code failed");
		throw JobHandleException("store source code failed");
	}
	fout << reply->str;
}

void JobBase::commitJudgeStatusToRedis(JudgeStatus status)
{
	static RedisCommand cmd("hset judge_status:%%d:%%d status %%d");
	try {
		cmd.execute(redisConn, jobType, sid, (int) status);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Commit judge status failed. Error information: ", e.what(), "; judge status: ", (int)status);
		throw;
	}
}
