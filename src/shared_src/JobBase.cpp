/*
 * JobBase.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */


#include "JobBase.hpp"
#include "logger.hpp"
#include "boost_format_suffix.hpp"
#include "redis_conn_factory.hpp"

#include <fstream>
#include <algorithm>

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_type_cast.hpp>

#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>


extern std::ostream log_fp;

std::pair<int, oj::s_id_type> JobBase::parseJobItem(const std::string & args)
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
		oj::s_id_type job_id(std::stoi(job_item.c_str() + cut_point + 1));
		return std::make_pair(job_type, job_id);
	} catch (const std::invalid_argument & ) {
		std::invalid_argument e("Invalid job_item arguments: " + job_item);
		EXCEPT_FATAL(0, 0, log_fp, "Parse job item failed.", e);
		throw e;
	}
}

JobBase::JobBase(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		jobType(jobType), s_id(s_id)
{
	try {
		using boost::lexical_cast;
		using namespace kerbal::utility;
		namespace optional_ns = kerbal::data_struct;
		using namespace optional_ns;

		kerbal::redis_v2::hash h(redis_conn, "job_info:%d:%d"_fmt(jobType, s_id).str());
		std::vector<optional<std::string>> res = h.hmget(
				"pid",
				"lang",
				"cases");

		if (res[0].empty())
			throw std::runtime_error("lack pid");
		this->p_id = lexical_cast<oj::p_id_type>(res[0].ignored_get());

		if (res[1].empty())
			throw std::runtime_error("lack lang");
		this->lang = (Language) (lexical_cast<int>(res[1].ignored_get()));

		this->cases = res[2].empty() ? 1 : lexical_cast<int>(res[2].ignored_get());

	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Fail to fetch job details.", e);
		throw;
	}
}

kerbal::redis_v2::reply JobBase::get_source_code() const
{
	using namespace kerbal::redis;

	kerbal::redis_v2::reply reply;
	std::vector<std::string> argv = { "hget", "source_code:%d:%d"_fmt(jobType, s_id).str(), "source" };
	try {
		auto redis_conn_handler = sync_fetch_redis_conn();
		reply = redis_conn_handler->argv_execute(argv.begin(), argv.end());
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get source code failed!", e);
		throw;
	}
	if (reply.type() != kerbal::redis_v2::reply_type::STRING) {
		kerbal::redis_v2::unexpected_case_exception e(reply.type(), argv.begin(), argv.end());
		EXCEPT_FATAL(jobType, s_id, log_fp, "Get source code failed!", e);
		throw e;
	}
	return reply;
}

void JobBase::storeSourceCode(const std::string & parent_path_args, const std::string & file_name) const
{
	using namespace kerbal::redis;

	kerbal::redis_v2::reply reply = this->get_source_code();

	boost::filesystem::path parent_path = parent_path_args;
	boost::filesystem::create_directories(parent_path);

	std::ofstream fout((parent_path / (file_name + '.' + source_file_suffix(lang))).string(), std::ios::out);
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
	// status为枚举类，在 redis 存储时用其对应的序号表示
	std::vector<std::string> argv = { "hset", "judge_status:%d:%d"_fmt(jobType, s_id).str(), "status", std::to_string(int(status)) };
    auto redis_conn_handler = sync_fetch_redis_conn();
    redis_conn_handler->argv_execute(argv.begin(), argv.end());
}
catch (const std::exception & e) {
	EXCEPT_FATAL(jobType, s_id, log_fp, "Commit judge status failed.", e, ", judge status: ", status);
	throw;
}
