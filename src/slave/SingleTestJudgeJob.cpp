/*
 * SingleTestJudgeJob.cpp
 *
 *  Created on: 2018年12月11日
 *      Author: peter
 */

#include "SingleTestJudgeJob.hpp"
#include "logger.hpp"
#include "Settings.hpp"

#include <string>

#include <boost/current_function.hpp>
#include <kerbal/redis_v2/hash.hpp>

extern std::ofstream log_fp;

SingleTestJudgeJob::SingleTestJudgeJob(int jobType, ojv4::s_id_type s_id, kerbal::redis_v2::connection & redis_conn) :
		jobType(jobType), s_id(s_id)
{

	kerbal::redis_v2::hash job_info(redis_conn, "job_info:%d:%d"_fmt(jobType, s_id).str());

	try {
		const auto [p_id, lang, time_limit, memory_limit, similarity_threshold, no_store_ac_code] =
				job_info.hmget_tuple<optional<ojv4::p_id_type>,
									optional<int>,
									optional<int>,
									optional<int>,
									optional<int>,
									optional<int>
									>("pid", "lang", "time_limit", "memory_limit", "sim_threshold", "no_store_ac_code");

		this->p_id = p_id.value();
		switch (static_cast<Language>(lang.value())) {
			case Language::C:
				this->languageStrategy.reset(new LanguageStrategy<Language::C>());
				break;
			case Language::Cpp:
				this->languageStrategy.reset(new LanguageStrategy<Language::Cpp>());
				break;
			case Language::Cpp14:
				this->languageStrategy.reset(new LanguageStrategy<Language::Cpp14>());
				break;
			case Language::Java:
				this->languageStrategy.reset(new LanguageStrategy<Language::Java>());
				break;
		}

		this->time_limit = ojv4::s_time_in_milliseconds(time_limit.value());
		this->memory_limit = ojv4::s_mem_in_MB(memory_limit.value());
		this->similarity_threshold = similarity_threshold.value();

	} catch(const std::bad_optional_access & e) {
		throw;
	}
}

void SingleTestJudgeJob::handle()
{
	this->store_source_code_procedure();

	UnitedJudgeResult judge_result = UnitedJudgeResult::ACCEPTED;

	this->compile_procedure(judge_result);

	ProtectedProcessDetails running_details;
	this->running_procedure(judge_result, running_details);

	const auto [running_result, real_time, cpu_time, memory, exit_code] = running_details;

//	this->commitJudgeResultToRedis(judgeResult, similarity_percentage);

//	kerbal::redis_v2::connection redis_conn;
//	//update update_queue
//	kerbal::redis_v2::list update_queue(redis_conn);
//	update_queue.rpush("%d,%d"_fmt(jobType, s_id));

#ifndef DEBUG
	// clear the dir
	this->clean_job_dir();
#endif

}

void SingleTestJudgeJob::store_source_code_procedure()
{
	kerbal::redis_v2::reply reply;

	try {
		kerbal::redis_v2::connection redis_conn;
		reply = redis_conn.execute("hget source_code:%d:%d source", this->jobType, this->s_id.to_literal());
	} catch (const std::exception & e) {
		throw std::runtime_error("Get source code failed!");
	}

	if (reply.type() != kerbal::redis_v2::reply_type::STRING) {
		throw std::runtime_error("Get source code failed!");
	}

	try {
		this->languageStrategy->save_source_code(this->working_dir, "Main", reply->str);
	} catch (const std::exception & e) {
		throw std::runtime_error("Save source code failed!");
	}
}

void SingleTestJudgeJob::compile_procedure(UnitedJudgeResult & judge_result)
{
	if (judge_result != UnitedJudgeResult::ACCEPTED) {
		return;
	}

	using namespace kerbal::compatibility::chrono_suffix;
	using namespace kerbal::utility;
	using namespace std::string_literals;

	ProtectedProcessConfig compile_config("/dev/null", this->working_dir / "compiler.out", this->working_dir / "compiler.out");
	compile_config.set_max_cpu_time(15_s);
	compile_config.set_max_real_time(15_s);
	compile_config.set_max_stack(1024_MB);
	compile_config.set_max_output_size(128_MB);

	boost::filesystem::path source_file_path = this->working_dir / ("Main."s + languageStrategy->default_suffix());
	const auto [running_result, real_time, cpu_time, memory, exit_code] =
			languageStrategy->compile(source_file_path, compile_config);

	switch (running_result) {
		case ProtectedProcessResult::ACCEPTED:
			break;
		case ProtectedProcessResult::REAL_TIME_LIMIT_EXCEEDED:
		case ProtectedProcessResult::CPU_TIME_LIMIT_EXCEEDED:
		case ProtectedProcessResult::MEMORY_LIMIT_EXCEEDED:
		case ProtectedProcessResult::OUTPUT_LIMIT_EXCEEDED:
			judge_result = UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED;
			break;
		case ProtectedProcessResult::RUNTIME_ERROR:
			judge_result = UnitedJudgeResult::COMPILE_ERROR;
			break;
		case ProtectedProcessResult::SYSTEM_ERROR:
			judge_result = UnitedJudgeResult::SYSTEM_ERROR;
			break;
	}
}

void SingleTestJudgeJob::running_procedure(UnitedJudgeResult & judge_result, ProtectedProcessDetails & running_details)
{
	if (judge_result != UnitedJudgeResult::ACCEPTED) {
		return;
	}

	using namespace kerbal::compatibility::chrono_suffix;
	using namespace kerbal::utility;
	using namespace std::string_literals;

	boost::filesystem::path input_file = Settings::JudgeSettings::input_dir() / to_string(p_id) / "in.1";
	ProtectedProcessConfig running_config(input_file, this->working_dir / "user.out", "/dev/null");
	running_config.set_max_cpu_time(this->time_limit);
	running_config.set_max_real_time(this->time_limit.value_or(15_s) + 1_s);
	running_config.set_max_stack(this->memory_limit);
	running_config.set_max_memory(this->memory_limit);
	running_config.set_max_output_size(32_MB);

	running_details = languageStrategy->running(this->working_dir / "Main", running_config);

	switch (running_details.running_result()) {
		case ProtectedProcessResult::ACCEPTED:
			judge_result = UnitedJudgeResult::WRONG_ANSWER;
//			judge_result = compare();
			break;
		case ProtectedProcessResult::REAL_TIME_LIMIT_EXCEEDED:
			judge_result = UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED;
			break;
		case ProtectedProcessResult::CPU_TIME_LIMIT_EXCEEDED:
			judge_result = UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED;
			break;
		case ProtectedProcessResult::MEMORY_LIMIT_EXCEEDED:
			judge_result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
			break;
		case ProtectedProcessResult::OUTPUT_LIMIT_EXCEEDED:
			judge_result = UnitedJudgeResult::OUTPUT_LIMIT_EXCEEDED;
			break;
		case ProtectedProcessResult::RUNTIME_ERROR:
			judge_result = UnitedJudgeResult::RUNTIME_ERROR;
			break;
		case ProtectedProcessResult::SYSTEM_ERROR:
			judge_result = UnitedJudgeResult::SYSTEM_ERROR;
			break;
	}
}

void SingleTestJudgeJob::compare_procedure(UnitedJudgeResult & judge_result)
{
	if (judge_result != UnitedJudgeResult::ACCEPTED) {
		return;
	}

}

bool SingleTestJudgeJob::clean_job_dir() noexcept try
{
	boost::filesystem::remove_all(this->working_dir);
	return true;
} catch (...) {
	return false;
}


void SingleTestJudgeJob::commit_judge_result_to_redis(kerbal::redis_v2::connection & redis_conn,
		const UnitedJudgeResult & judge_result,
		const ojv4::s_time_in_milliseconds & cpu_time,
		const ojv4::s_time_in_milliseconds & real_time,
		const ojv4::s_mem_in_byte & memory,
		int similarity_percentage)
try {
	kerbal::redis_v2::hash judge_status(redis_conn, "judge_status:%d:%d"_fmt(jobType, s_id).str());
	judge_status.hmset(
			"status", (int) JudgeStatus::FINISHED,
			"result", (int) judge_result,
			"cpu_time", cpu_time.count(),
			"real_time", real_time.count(),
			"memory", memory.count(),
			"similarity_percentage", similarity_percentage,
			"judge_server_id", Settings::JudgerRuntimeVariable::judge_server_id());
} catch (const std::exception & e) {

	throw;
}
