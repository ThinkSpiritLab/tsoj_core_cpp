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

std::pair<int, int> JobBase::parseJobItem(const std::string & args)
{
	std::string job_item = args;
	std::string::size_type cut_point = job_item.find(',');

	if (cut_point == std::string::npos) {
		std::invalid_argument e("Invalid job_item arguments: " + job_item);
		EXCEPT_FATAL(0, 0, log_fp, "Parse job item failed.", e);
		throw e;
	}

	job_item[cut_point] = '\0';

	try {
		int job_type = std::stoi(job_item.c_str());
		int job_id = std::stoi(job_item.c_str() + cut_point + 1);
		return std::make_pair(job_type, job_id);
	} catch (const std::invalid_argument & ) {
		std::invalid_argument e("Invalid job_item arguments: " + job_item);
		EXCEPT_FATAL(0, 0, log_fp, "Parse job item failed.", e);
		throw e;
	}
}

JobBase::JobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn) :
		jobType(jobType), sid(sid), redisConn(redisConn)
{
	using std::chrono::milliseconds;
	using kerbal::utility::MB;
	static boost::format key_name_tmpl("job_info:%d:%d");
	const std::string job_info_key = (key_name_tmpl % jobType % sid).str();

	kerbal::redis::Operation opt(redisConn);

	try {
		this->pid = opt.hget<int>(job_info_key, "pid");
		this->lang = (Language) (opt.hget<int>(job_info_key, "lang"));
		this->cases = opt.hget<int>(job_info_key, "cases");
		this->timeLimit = milliseconds(opt.hget<int>(job_info_key, "time_limit"));
		this->memoryLimit = MB(opt.hget<unsigned long long>(job_info_key, "memory_limit"));
		this->similarity_threshold = opt.hget<int>(job_info_key, "sim_threshold");
	} catch (const RedisNilException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Job details lost.", e);
		throw;
	} catch (const RedisUnexpectedCaseException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Redis returns an unexpected type.", e);
		throw;
	} catch (const RedisException & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Fail to fetch job details.", e);
		throw;
	}
}

kerbal::redis::RedisReply JobBase::get_source_code() const
{
	static RedisCommand get_src_code_templ("hget source_code:%d:%d source");
	RedisReply reply;
	try {
		reply = get_src_code_templ.execute(redisConn, this->jobType, this->sid);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, sid, log_fp, "Get source code failed!", e);
		throw;
	}
	if (reply.replyType() != RedisReplyType::STRING) {
		RedisUnexpectedCaseException e(reply.replyType());
		EXCEPT_FATAL(jobType, sid, log_fp, "Get source code failed!", e);
		throw e;
	}
	return reply;
}

void JobBase::storeSourceCode() const
{
	RedisReply reply = this->get_source_code();

	static constexpr const char * stored_file_name[] = {"Main.c", "Main.cpp", "Main.java"};
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
		LOG_FATAL(jobType, sid, log_fp, "Open source code file failed");
		throw JobHandleException("Open source code file failed");
	}
	fout << reply->str;
	if (fout.bad()) {
		LOG_FATAL(jobType, sid, log_fp, "Store source code failed");
		throw JobHandleException("Store source code failed");
	}
}

void JobBase::commitJudgeStatusToRedis(JudgeStatus status) try
{
	static RedisCommand cmd("hset judge_status:%d:%d status %d");
	// status为枚举类，在 redis 存储时用其对应的序号表示
	cmd.execute(redisConn, jobType, sid, (int) status);
} catch (const std::exception & e) {
	LOG_FATAL(jobType, sid, log_fp, "Commit judge status failed. ",
			"Error information: ", e.what(), ", ",
			"judge status: ", (int)status);
	throw;
}
