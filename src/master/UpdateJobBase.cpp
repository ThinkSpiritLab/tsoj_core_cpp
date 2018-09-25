/*
 * UpdateJobBase.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "UpdateJobBase.hpp"

#include "logger.hpp"
#include "boost_format_suffix.hpp"

extern std::ostream log_fp;

namespace
{
	using namespace kerbal::redis;
	using namespace mysqlpp;
}


void UpdateJobBase::fetchDetailsFromRedis()
{
	supper_t::fetchDetailsFromRedis();

	using std::stoi;
	static boost::format key_name_tmpl("job_info:%d:%d");

	std::vector<std::string> query_res;
	Operation opt(redisConn);

	try {
		query_res = opt.hmget((key_name_tmpl % jobType % sid).str(), "uid", "cid", "post_time", "have_accepted", "no_store_ac_code", "is_rejudge");
	} catch (const RedisNilException & e) {
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
		throw;
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
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", UNKNOWN_EXCEPTION_WHAT);
		throw;
	}

	try {
		this->uid = stoi(query_res[0]);
		this->cid = stoi(query_res[1]);
		this->postTime = query_res[2];
		this->haveAccepted = (bool) stoi(query_res[3]);
		this->no_store_ac_code = (bool) stoi(query_res[4]);
		this->is_rejudge = (bool) stoi(query_res[5]);
	} catch (const std::exception & e) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", e.what());
		throw;
	} catch (...) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", UNKNOWN_EXCEPTION_WHAT);
		throw;
	}
}

UpdateJobBase::UpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
		std::unique_ptr<Connection> && mysqlConn) : supper_t(jobType, sid, redisConn), mysqlConn(std::move(mysqlConn))
{
	this->UpdateJobBase::fetchDetailsFromRedis();
}

void UpdateJobBase::handle()
{

	this->update_solution();

	{ // update source code
		RedisReply reply = this->get_source_code();
		this->update_source_code(reply->str);
	}
	this->update_user_and_problem();
	this->update_user_problem();

	//保存编译信息
	if (result.judge_result == UnitedJudgeResult::COMPILE_ERROR) {
		RedisReply reply = this->get_compile_info();
		this->update_compile_info(reply->str);
	}

	this->commitJudgeStatusToRedis(JudgeStatus::UPDATED);

	// del solution - 600
}

void UpdateJobBase::update_source_code(const char * source_code)
{
	if (source_code == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty source code!");
		return;
	}

	std::string source_code_table;
	if (jobType == 0) {
		source_code_table = "source";
	} else {
		source_code_table = "contest_source%d"_fmt(jobType).str();
	}
	mysqlpp::Query insert = mysqlConn->query(
			"insert into %0 (s_id, source_code)"
			"values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(source_code_table, sid, source_code);
	if (!res) {
		throw MysqlEmptyResException(insert.errnum(), insert.error());
	}
}

void UpdateJobBase::update_compile_info(const char * compile_info)
{
	if (compile_info == nullptr) {
		LOG_WARNING(jobType, sid, log_fp, "empty compile info!");
		return;
	}

	std::string compile_info_table;
	if (jobType == 0) {
		compile_info_table = "compile_info";
	} else {
		compile_info_table = "contest_compile_info%d"_fmt(jobType).str();
	}

	mysqlpp::Query insert = mysqlConn->query(
			"insert into %0 (s_id, compile_error_info) values (%1, %2q)"
	);
	insert.parse();
	mysqlpp::SimpleResult res = insert.execute(compile_info_table, sid, compile_info);
	if (!res) {
		throw MysqlEmptyResException(insert.errnum(), insert.error());
	}
}

kerbal::redis::RedisReply UpdateJobBase::get_source_code() const
{
	static RedisCommand get_src_code_templ("hget source_code:%d:%d source");
	RedisReply reply;
	try {
		reply = get_src_code_templ.execute(redisConn, this->jobType, this->sid);
	} catch (std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Get source code failed! Error information: ", e.what());
		throw;
	}
	if (reply.replyType() != RedisReplyType::STRING) {
		LOG_FATAL(jobType, sid, log_fp, "Get source code failed! Error information: ", "unexpected redis reply type");
		throw RedisUnexpectedCaseException(reply.replyType());
	}
	return reply;
}

kerbal::redis::RedisReply UpdateJobBase::get_compile_info() const
{
	static RedisCommand get_ce_info_templ("get compile_info:%d:%d");
	RedisReply reply;
	try {
		reply = get_ce_info_templ.execute(redisConn, this->jobType, this->sid);
	} catch (std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Get compile info failed! Error information: ", e.what());
		throw;
	}
	if (reply.replyType() != RedisReplyType::STRING) {
		LOG_FATAL(jobType, sid, log_fp, "Get compile info failed! Error information: ", "unexpected redis reply type");
		throw RedisUnexpectedCaseException(reply.replyType());
	}
	return reply;
}

void UpdateJobBase::core_update_failed_table() noexcept try
{
	mysqlpp::Query insert = mysqlConn->query(
			"insert into core_update_failed "
			"(type, job_id, pid, uid, lang, "
			"post_time, cid, cases, time_limit, memory_limit, "
			"sim_threshold, result, cpu_time, memory, sim_percentage, source_code, compile_error_info) "
			"values (%0, %1, %2, %3, %4, "
			"%5q, %6, %7, %8, %9, "
			"%10, %11, %12, %13, %14, %15q, %16q)"
	);
	insert.parse();

	RedisReply source_code = this->get_source_code();
	RedisReply compile_error_info = this->get_compile_info();

	mysqlpp::SimpleResult res = insert.execute(
		jobType, sid, pid, uid, (int) lang,
		postTime, cid, cases, timeLimit.count(), memoryLimit.count(),
		similarity_threshold, (int) result.judge_result, result.cpu_time.count(),
		result.memory.count(), result.similarity_percentage, source_code->str, compile_error_info->str
	);

	if (!res) {
		throw MysqlEmptyResException(insert.errnum(), insert.error());
	}
} catch(const std::exception & e) {
	LOG_FATAL(jobType, sid, log_fp, "Update failed table failed! Error information: ", e.what());
} catch(...) {
	LOG_FATAL(jobType, sid, log_fp, "Update failed table failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
}
