/*
 * JobBase.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */


#include "JobBase.hpp"
#include "logger.hpp"
#include "mkdir_p.hpp"

#include <fstream>
#include <algorithm>

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_type_cast.hpp>

#include <boost/format.hpp>


extern std::ostream log_fp;

std::pair<int, ojv4::s_id_type> JobBase::parseJobItem(const std::string & args)
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
		ojv4::s_id_type job_id(std::stoi(job_item.c_str() + cut_point + 1));
		return std::make_pair(job_type, job_id);
	} catch (const std::invalid_argument & ) {
		std::invalid_argument e("Invalid job_item arguments: " + job_item);
		EXCEPT_FATAL(0, 0, log_fp, "Parse job item failed.", e);
		throw e;
	}
}

JobBase::JobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn) :
		jobType(jobType), s_id(s_id), redisConn(redisConn)
{
	using namespace kerbal::redis;
	using std::chrono::milliseconds;
	using kerbal::utility::MB;
	static boost::format key_name_tmpl("job_info:%d:%d");
	const std::string job_info_key = (key_name_tmpl % jobType % s_id).str();

	kerbal::redis::Operation opt(redisConn);

	try {
		this->p_id = ojv4::p_id_type(opt.hget<ojv4::p_id_literal>(job_info_key, "pid"));
		this->lang = (Language) (opt.hget<int>(job_info_key, "lang"));
		this->cases = opt.hget<int>(job_info_key, "cases");
		this->timeLimit = milliseconds(opt.hget<ojv4::s_time_literal>(job_info_key, "time_limit"));
		this->memoryLimit = MB(opt.hget<ojv4::s_mem_literal>(job_info_key, "memory_limit"));
		this->similarity_threshold = opt.hget<int>(job_info_key, "sim_threshold");
	} catch (const RedisNilException & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Job details lost.", e);
		throw;
	} catch (const RedisUnexpectedCaseException & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Redis returns an unexpected type.", e);
		throw;
	} catch (const RedisException & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Fail to fetch job details.", e);
		throw;
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Fail to fetch job details.", e);
		throw;
	}
}

kerbal::redis::RedisReply JobBase::get_source_code() const
{
	using namespace kerbal::redis;

	static RedisCommand get_src_code_templ("hget source_code:%d:%d source");
	RedisReply reply;
	try {
		reply = get_src_code_templ.execute(redisConn, this->jobType, this->s_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get source code failed!", e);
		throw;
	}
	if (reply.replyType() != RedisReplyType::STRING) {
		RedisUnexpectedCaseException e(reply.replyType());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get source code failed!", e);
		throw e;
	}
	return reply;
}

void JobBase::storeSourceCode(const std::string & parent_path_args, const std::string & file_name) const
{
	using namespace kerbal::redis;

	RedisReply reply = this->get_source_code();

	std::string parent_path = parent_path_args;
	while(!parent_path.empty() && std::isblank(parent_path.back())) {
		parent_path.pop_back();
	}
	if (parent_path.size() == 0) {
		parent_path = "./";
	} else {
		if (parent_path.back() != '/') {
			parent_path += '/';
		}
		mkdir_p(parent_path);
	}

	std::ofstream fout(parent_path + file_name + '.' + source_file_suffix(lang), std::ios::out);
	if (!fout) {
		LOG_FATAL(jobType, s_id, log_fp, "Open source code file failed");
		throw JobHandleException("Open source code file failed");
	}
	fout << reply->str;
	if (fout.bad()) {
		LOG_FATAL(jobType, s_id, log_fp, "Store source code failed");
		throw JobHandleException("Store source code failed");
	}
}

void JobBase::commitJudgeStatusToRedis(JudgeStatus status) try
{
	using namespace kerbal::redis;

	static RedisCommand cmd("hset judge_status:%d:%d status %d");
	// status为枚举类，在 redis 存储时用其对应的序号表示
	cmd.execute(redisConn, jobType, s_id, (int) status);
} catch (const std::exception & e) {
	EXCEPT_FATAL(jobType, s_id, log_fp, "Commit judge status failed.", e, ", judge status: ", status);
	throw;
}
