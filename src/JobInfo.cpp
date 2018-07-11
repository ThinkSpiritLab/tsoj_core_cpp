/*
 * JobInfo.cpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#include "JobInfo.hpp"
#include "Config.hpp"
#include "global_shared_variable.hpp"
#include "logger.hpp"
#include "compare.hpp"
#include "process.hpp"
#include "rules/seccomp_rules.hpp"

#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

#include <cstring>
#include <stdio.h>
#include <dirent.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_type_cast.hpp>

#include <boost/format.hpp>

using namespace kerbal::redis;
using kerbal::redis::Context;

std::pair<int, int> JobInfo::parser_job_item(const std::string & args)
{
	std::string job_item = args;
	std::string::size_type cut_point = job_item.find(',');

	if (cut_point == std::string::npos) {
		throw std::invalid_argument("invalid job_item arguments: " + job_item);
	}

	job_item[cut_point] = '\0';

	try {
		int job_type = std::stoi(job_item.c_str());
		int job_id = std::stoi(job_item.c_str() + cut_point + 1);
		return std::make_pair(job_type, job_id);
	} catch (const std::invalid_argument & e) {
		LOG_FATAL(0, 0, log_fp, "Invalid job_item arguments: ", job_item);
		throw std::invalid_argument("Invalid job_item arguments");
	}
}

JobInfo::JobInfo(int jobType, int sid)
{
	this->jobType = jobType;
	this->sid = sid;
}

JobInfo JobInfo::fetchFromRedis(const Context & conn, int jobType, int sid)
{
	using std::stoi;


	JobInfo res(jobType, sid);
	std::unordered_map<std::string, std::string> query_res;
	try {
		static Operation opt;
		static boost::format key_name_tmpl("job_info:%d:%d");
		query_res = opt.hgetall(conn, (key_name_tmpl % jobType % sid).str());
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
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", "unknown exception");
		throw;
	}

	static boost::format dir_templ(init_dir + "/job-%d-%d");
	res.dir = (dir_templ % jobType % sid).str();

	try {
		res.pid = stoi(query_res.at("pid"));
		res.uid = stoi(query_res.at("uid"));
		res.lang = (Language) stoi(query_res.at("lang"));
		res.cases = stoi(query_res.at("cases"));
		res.cid = stoi(query_res.at("cid"));
		res.timeLimit = std::chrono::milliseconds(stoi(query_res.at("time_limit")));
		res.memoryLimit = kerbal::utility::MB(stoull(query_res.at("memory_limit")));
		res.postTime = query_res.at("post_time");
		res.haveAccepted = stoi(query_res.at("have_accepted"));
		res.no_store_ac_code = (bool) stoi(query_res.at("no_store_ac_code"));
		res.is_rejudge = (bool) stoi(query_res.at("is_rejudge"));
	} catch (const RedisNilException & e) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", e.what());
		throw;
	} catch (const std::exception & e) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", e.what());
		throw;
	} catch (...) {
		LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", "unknown exception");
		throw;
	}

	return res;
}


void JobInfo::judge_job(const Context & conn)
{
    LOG_DEBUG(jobType, sid, log_fp, "judge start");

    this->change_job_dir();
    this->store_source_code(conn);

    LOG_DEBUG(jobType, sid, log_fp, "store source code finished");

	// compile
    this->commitJudgeStatusToRedis(conn, JudgeStatus::COMPILING);
	LOG_DEBUG(jobType, sid, log_fp, "compile start");
	Result compile_result = this->compile();

	LOG_DEBUG(jobType, sid, log_fp, "compile finished");
	LOG_DEBUG(jobType, sid, log_fp, "compile result: ", compile_result);

	switch (compile_result.result) {
		case UnitedJudgeResult::ACCEPTED: {
			LOG_DEBUG(jobType, sid, log_fp, "compile success");
			Result result = this->running(conn);
			LOG_DEBUG(jobType, sid, log_fp, "judging finished");
			this->commitJudgeResultToRedis(conn, result);
			break;
		}
		case UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED:
			LOG_WARNING(jobType, sid, log_fp, "compile resource limit exceed");
			this->commitJudgeResultToRedis(conn, compile_result);
			break;
		case UnitedJudgeResult::SYSTEM_ERROR:
			LOG_FATAL(jobType, sid, log_fp, "system error");
			this->commitJudgeResultToRedis(conn, compile_result);
			break;
		case UnitedJudgeResult::COMPILE_ERROR:
		default:
			LOG_DEBUG(jobType, sid, log_fp, "compile failed");
			this->commitJudgeResultToRedis(conn, compile_result);

			if (this->set_compile_info(conn) == false) {
				LOG_WARNING(jobType, sid, log_fp, "An error occurred while getting compile info.");
			}
			break;
	}

	//update update_queue
	static RedisCommand update_queue("rpush update_queue %%d,%%d");
	update_queue.execute(conn, jobType, sid);

#ifndef DEBUG
    // clear the dir
    this->clean_job_dir();
#endif

}

Result JobInfo::running(const Context & conn)
{
	Config _config(*this, Config::RUNNING);
//	this->commitJudgeStatusToRedis(conn, "cases_finished", 0);

	Result result;
	this->commitJudgeStatusToRedis(conn, JudgeStatus::RUNNING);
	for (int i = 1; i <= cases; ++i) {
		// change input path
		std::string input_path = (boost::format("%s/%d/in.%d") % input_dir % pid % i).str();
		_config.input_path = input_path.c_str();

		LOG_DEBUG(jobType, sid, log_fp, "running case: ", i);
		Result current_running_result = this->execute(_config);
		LOG_DEBUG(jobType, sid, log_fp, "running case: ", i, " finished; result: ", current_running_result);
		if (current_running_result.result == UnitedJudgeResult::ACCEPTED) {
			// change answer path
			std::string answer = (boost::format("%s/%d/out.%d") % input_dir % pid % i).str();
			this->commitJudgeStatusToRedis(conn, JudgeStatus::JUDGING);
			// compare
			LOG_DEBUG(jobType, sid, log_fp, "comparing case: ", i);
			UnitedJudgeResult compare_result = compare(answer.c_str(), _config.output_path);
			LOG_DEBUG(jobType, sid, log_fp, "comparing case: ", i, " finished; compare result: ", compare_result);
			if (compare_result == UnitedJudgeResult::ACCEPTED) {
				result = current_running_result;
			} else {
				current_running_result.result = compare_result;
				return current_running_result;
			}
		} else {
			return current_running_result;
		}
		//set_judge_status(conn, type, job_id, "cases_finished", i);
	}
	LOG_DEBUG(jobType, sid, log_fp, "all cases accepted");
	return result;
}

void JobInfo::commitJudgeStatusToRedis(const Context & conn, JudgeStatus status)
{
	static RedisCommand cmd("hset judge_status:%%d:%%d status %%d");
	try {
		cmd.execute(conn, jobType, sid, (int) status);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Commit judge status failed. Error information: ", e.what(), "; judge status: ", (int)status);
		throw;
	}
}

void JobInfo::commitJudgeResultToRedis(const Context & conn, const Result & result) const
{
	static Operation opt;
	static boost::format judge_status_name_tmpl("judge_status:%d:%d");
	try {
		opt.hmset(conn, (judge_status_name_tmpl % jobType % sid).str(),
				"status", (int) JudgeStatus::FINISHED,
				"result", (int) result.result,
				"cpu_time", result.cpu_time.count(),
				"real_time", result.real_time.count(),
				"memory", (kerbal::utility::Byte(result.memory)).count(),
				"similarity_percentage", 0,
				"judge_server_id", judge_server_id);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Commit judge result failed. Error information: ", e.what(), "; judge result: ", result);
		throw;
	}
}

bool JobInfo::set_compile_info(const Context & conn) noexcept
{
	std::ifstream fin("compiler.out", std::ios::in);
	if (!fin) {
		LOG_FATAL(jobType, sid, log_fp, "Cannot open compiler.out");
		return false;
	}

	// get length of file:
	fin.seekg(0, fin.end);
	int length = fin.tellg();
	fin.seekg(0, fin.beg);

	char buffer[MYSQL_TEXT_MAX_SIZE + 10];
	fin.read(buffer, MYSQL_TEXT_MAX_SIZE);

	if (!fin) {
		LOG_FATAL(jobType, sid, log_fp, "Read compile info error, only ", fin.gcount(), " could be read");
		return false;
	}

	 if (MYSQL_TEXT_MAX_SIZE >= 3 && length > MYSQL_TEXT_MAX_SIZE) {
		char * end = buffer + MYSQL_TEXT_MAX_SIZE - 3;
		for (int i = 0; i < 3; ++i) {
			*end = '.';
			++end;
		}
	}

	try {
		static RedisCommand cmd("set compile_info:%%d:%%d %%s");
		cmd.execute(conn, jobType, sid, buffer);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Set compile info failed. Error information: ", e.what());
		return false;
	}

	return true;
}


int JobInfo::calculate_similarity() const
{
	std::string sim_cmd;
	switch (this->lang) {
		case Language::C:
			sim_cmd = (boost::format("sim_c -S -P Main.c / %s/%d/0/* > sim.txt") % accepted_solutions_dir % this->pid).str();
			break;
		case Language::Cpp:
		case Language::Cpp14:
			sim_cmd = (boost::format("sim_c -S -P Main.cpp / %s/%d/0/* > sim.txt") % accepted_solutions_dir % this->pid).str();
			break;
		case Language::Java:
			sim_cmd = (boost::format("sim_java -S -P Main.java / %s/%d/2/* > sim.txt") % accepted_solutions_dir % this->pid).str();
			break;
	}

	int sim_cmd_return_value = system(sim_cmd.c_str());
	if (-1 == sim_cmd_return_value) {
		throw JobHandleException("sim execute failed");
	}
	if (!WIFEXITED(sim_cmd_return_value) || WEXITSTATUS(sim_cmd_return_value)) {
		throw JobHandleException("sim command exits incorrectly, exit status: " + std::to_string(WEXITSTATUS(sim_cmd_return_value)));
	}

	std::ifstream fp("sim.txt", std::ios::in);

	if (!fp) {
		throw JobHandleException("sim.txt open failed");
	}

	std::string buf;
	while (std::getline(fp, buf)) {
		if (buf.empty()) {
			break;
		}
	}

	int max_similarity = 0;
	while (std::getline(fp, buf)) {
		std::string::reverse_iterator en = std::find(buf.rbegin(), buf.rend(), '%');
		if (en != buf.rend()) {
			do {
				++en;
			} while (std::isspace(*en));
			std::string::reverse_iterator be = std::find_if_not(en, buf.rend(), ::isdigit);
			--en;
			--be;
			try {
				int now_similarity = std::stoi(buf.substr(buf.length() - (std::distance(buf.rbegin(), be)) - 1, std::distance(en, be)));
				max_similarity = std::max(max_similarity, now_similarity);
			} catch (const std::invalid_argument & e) {
				throw JobHandleException("an error occurred while calculate max similarity");
			}
		}
	}

	return max_similarity;
}


void JobInfo::store_code_to_accepted_solutions_dir() const
{
// 将已经AC且no_store_ac_code标志不为1的代码存入accepted_solutions_dir目录
	std::string code_path = accepted_solutions_dir;

	// 以下逐个检查文件夹可写权限
	if (access(code_path.c_str(), W_OK) != 0) {
		LOG_FATAL(jobType, sid, log_fp, "accepted_solutions_dir does not have write permission");
		throw JobHandleException("accepted_solutions_dir does not have write permission");
	}
	code_path += "/" + std::to_string(pid);
	if (access(code_path.c_str(), W_OK) != 0) {
		if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Cannot create directory: ", code_path);
			throw JobHandleException("Cannot create directory");
		}
	}
	switch (lang) {
		case Language::C:
		case Language::Cpp:
		case Language::Cpp14:
			code_path += "/0";
			break;
		case Language::Java:
			code_path += "/2";
			break;
	}
	if (access(code_path.c_str(), W_OK) != 0) {
		if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Cannot create directory: ", code_path);
			throw JobHandleException("Cannot create directory");
		}
	}

	// 检查本题已经保存的AC代码份数是否超过了stored_ac_code_max_num
	DIR *dir = opendir(code_path.c_str());
	if (dir == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "Cannot open directory ", code_path, " for checking the number of accepted code");
		throw JobHandleException("Cannot open directory for checking the number of accepted code");
	}

	int filecnt = 0;
	while (readdir(dir) != NULL) {
		++filecnt;
	}
	// 因为opendir能读到"."和".."，所以要+2
	if (filecnt > stored_ac_code_max_num + 2) {
		rewinddir(dir);
		std::vector<int> arr;
		arr.reserve(filecnt);
		struct dirent *ptr = NULL;
		while ((ptr = readdir(dir)) != NULL) {
			if (ptr->d_name != std::string(".") && ptr->d_name != std::string("..")) {
				arr.push_back(std::stoi(ptr->d_name));
			}
		}
		std::partial_sort(arr.begin(), arr.end() - stored_ac_code_max_num, arr.end());

		for (std::vector<int>::iterator it = arr.begin(); it != arr.end() - stored_ac_code_max_num; ++it) {
			switch (lang) {
				case Language::C:
				case Language::Cpp:
				case Language::Cpp14: {
					std::string file_path = code_path + "/" + std::to_string(*it) + ".c";
					unlink(file_path.c_str());
					file_path = code_path + "/" + std::to_string(*it) + ".cpp";
					unlink(file_path.c_str());
					break;
				}
				case Language::Java: {
					std::string file_path = code_path + "/" + std::to_string(*it) + ".java";
					unlink(file_path.c_str());
					break;
				}
			}
		}
	}
	closedir(dir);

	// 开始复制AC代码
	std::string copy_cmd;
	switch (lang) {
		case Language::C:
			copy_cmd = "cp Main.c " + code_path + "/" + std::to_string(sid) + ".c";
			break;
		case Language::Cpp:
		case Language::Cpp14:
			copy_cmd = "cp Main.cpp " + code_path + "/" + std::to_string(sid) + ".cpp";
			break;
		case Language::Java:
			copy_cmd = "cp Main.java " + code_path + "/" + std::to_string(sid) + ".java";
			break;
	}

	int copy_cmd_return_value = system(copy_cmd.c_str());
	// 检查命令执行是否成功
	if (-1 == copy_cmd_return_value) {
		LOG_FATAL(jobType, sid, log_fp, "Fork failed while copying code to accepted_solutions_dir");
		throw JobHandleException("Fork failed while copying code to accepted_solutions_dir");
	}
	if (!WIFEXITED(copy_cmd_return_value) || WEXITSTATUS(copy_cmd_return_value)) {
		LOG_FATAL(jobType, sid, log_fp, "Error occurred while copying code to accepted_solutions_dir, exit status: ", WEXITSTATUS(copy_cmd_return_value));
		throw JobHandleException("Error occurred while copying code to accepted_solutions_dir");
	}
}

void JobInfo::change_job_dir() const
{
	// TODO
	LOG_DEBUG(jobType, sid, log_fp, "make dir: ", dir);
	mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	chmod(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	if (chdir(dir.c_str())) {
		LOG_FATAL(jobType, sid, log_fp, "can not change to job dir: [", dir, "]");
		throw JobHandleException("can not change to job dir");
	}
}

void JobInfo::clean_job_dir() const noexcept
{
	LOG_DEBUG(jobType, sid, log_fp, "clean dir");
	DIR *dp = opendir(".");
	if (dp == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "clean dir failed.");
		return;
	}
	struct dirent * entry = NULL;
	while ((entry = readdir(dp)) != NULL) {
		if (std::string(".") != entry->d_name && std::string("..") != entry->d_name) {
			unlink(entry->d_name);
		}
	}
	closedir(dp);
	if (chdir("..")) {
		LOG_FATAL(jobType, sid, log_fp, "can not go back to ../");
		return;
	}
	rmdir(dir.c_str());
}

void JobInfo::push_back_failed_judge_job(const Context & conn) const noexcept
{
	try {
		LOG_WARNING(jobType, sid, log_fp, "push back to judge failed list");
		Result result;
		result.result = UnitedJudgeResult::SYSTEM_ERROR;
		this->commitJudgeResultToRedis(conn, result);

		List<std::string> judge_failure_list(conn, "judge_failure_list");
		judge_failure_list.push_back(std::to_string(jobType) + ":" + std::to_string(sid));
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: ", e.what());
	} catch (...) {
		LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: ", "unknown exception");
	}
}

namespace
{
	std::chrono::milliseconds timevalToChrono(const timeval & val)
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(seconds(val.tv_sec) + microseconds(val.tv_usec));
	}
}

Result JobInfo::execute(const Config & config) const noexcept
{
	using namespace std::chrono;

	Result result;

	// check whether current user is root
	uid_t uid = getuid();
	if (uid != 0) {
		LOG_FATAL(jobType, sid, log_fp, "ROOT_REQUIRED");
		result.setErrorCode(RunnerError::ROOT_REQUIRED);
		return result;
	}

	// check args
	if (config.check_is_valid_config() == false) {
		LOG_FATAL(jobType, sid, log_fp, "INVALID_CONFIG");
		result.setErrorCode(RunnerError::INVALID_CONFIG);
		return result;
	}

	std::unique_ptr<Process> child_process;

	try {
		child_process.reset(new Process(true, [this, &config]() {
			this->child_process(config);
		}));
	} catch (std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Fork failed. Error information: ", e.what());
		result.setErrorCode(RunnerError::FORK_FAILED);
		return result;
	}

	// record current time
	time_point<high_resolution_clock> start(high_resolution_clock::now());

	if (config.max_real_time != Config::TIME_UNLIMITED) {
		try {
			// new thread to monitor process running time
			std::thread timeout_killer_thread([&child_process](milliseconds timeout) {
				static constexpr seconds one_second {1};
				std::this_thread::sleep_for (timeout + one_second);
				if (child_process->kill(SIGKILL) != 0) { //TODO log
						return;
					}
					return;
				}, config.max_real_time);
			try {
				timeout_killer_thread.detach();
			} catch (const std::system_error & e) {
				child_process->kill(SIGKILL);
				LOG_FATAL(jobType, sid, log_fp, "Thread detach failed. Error information: ", e.what(), " , error code: ", e.code().value());
				result.setErrorCode(RunnerError::PTHREAD_FAILED);
				return result;
			}
		} catch (const std::system_error & e) {
			child_process->kill(SIGKILL);
			LOG_FATAL(jobType, sid, log_fp, "Thread construct failed. Error information: ", e.what(), " , error code: ", e.code().value());
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		} catch (...) {
			child_process->kill(SIGKILL);
			LOG_FATAL(jobType, sid, log_fp, "Thread construct failed. Error information: ", "unknown exception");
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		}
		LOG_DEBUG(jobType, sid, log_fp, "Timeout thread work success");
	}

	int status;
	struct rusage resource_usage;

	// wait for child process to terminate
	// on success, returns the process ID of the child whose state has changed;
	// On error, -1 is returned.
	if (child_process->wait4(&status, WSTOPPED, &resource_usage) == -1) {
		child_process->kill(SIGKILL);
		LOG_FATAL(jobType, sid, log_fp, "WAIT_FAILED");
		result.setErrorCode(RunnerError::WAIT_FAILED);
		return result;
	}

	result.real_time = duration_cast<milliseconds>(high_resolution_clock::now() - start);
	result.cpu_time = timevalToChrono(resource_usage.ru_utime) + timevalToChrono(resource_usage.ru_stime);
	result.memory = kerbal::utility::KB(resource_usage.ru_maxrss);
	result.exit_code = WEXITSTATUS(status);

	if (result.exit_code != 0) {
		LOG_FATAL(jobType, sid, log_fp, "exit code != 0");
		result.result = UnitedJudgeResult::RUNTIME_ERROR;
		return result;
	}
	// if signaled
	if (WIFSIGNALED(status) != 0) {
		result.signal = WTERMSIG(status);
		LOG_DEBUG(jobType, sid, log_fp, "signal: ", result.signal);
		switch (result.signal) {
			case SIGSEGV:
				if (config.max_memory != Config::MEMORY_UNLIMITED && result.memory > config.max_memory) {
					result.result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
				} else {
					result.result = UnitedJudgeResult::RUNTIME_ERROR;
				}
				break;
			case SIGUSR1:
				result.result = UnitedJudgeResult::SYSTEM_ERROR;
				break;
			default:
				result.result = UnitedJudgeResult::RUNTIME_ERROR;
		}
	} else {
		if (config.max_memory != Config::MEMORY_UNLIMITED && result.memory > config.max_memory) {
			result.result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
		}
	}
	if (config.max_real_time != Config::TIME_UNLIMITED && result.real_time > config.max_real_time) {
		result.result = UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED;
	}
	if (config.max_cpu_time != Config::TIME_UNLIMITED && result.cpu_time > config.max_cpu_time) {
		result.result = UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED;
	}
	return result;
}

void JobInfo::store_source_code(const Context & conn) const
{
	static RedisCommand cmd("hget source_code:%%d:%%d source");
	AutoFreeReply reply;
	try {
		reply = cmd.execute(conn, jobType, sid);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Get source code failed. Error information: ", e.what());
		throw JobHandleException("Get source code failed");
	}

	static const std::string stored_file_name[] = { "Main.c", "Main.cpp", "Main.java" };
	const std::string * file_name = nullptr;
	switch (lang) {
		case Language::C:
			file_name = stored_file_name;
			break;
		case Language::Cpp:
		case Language::Cpp14:
			file_name = stored_file_name + 1;
			break;
		case Language::Java:
			file_name = stored_file_name + 2;
			break;
	}
	std::ofstream fout(*file_name, std::ios::out);
	if (!fout) {
		LOG_FATAL(jobType, sid, log_fp, "store source code failed");
		throw JobHandleException("store source code failed");
	}
	fout << reply->str;
}

Result JobInfo::compile() const noexcept
{
	Result result = this->execute(Config(*this, Config::COMPILE));

	switch (result.result) {
		case UnitedJudgeResult::ACCEPTED:
			return result;
		case UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED:
		case UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED:
		case UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED:
			result.result = UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED;
			return result;
		case UnitedJudgeResult::RUNTIME_ERROR:
			result.result = UnitedJudgeResult::COMPILE_ERROR;
			return result;
		case UnitedJudgeResult::SYSTEM_ERROR:
			LOG_FATAL(jobType, sid, log_fp, "System error occurred while compiling. Execute result: ", result);
			return result;
		default:
			LOG_FATAL(jobType, sid, log_fp, "Unexpected compile result: ", result);
			result.result = UnitedJudgeResult::COMPILE_ERROR;
			return result;
	}
}

namespace
{
	std::shared_ptr<FILE> open_file(const char * file_name, const char * modes)
	{
		FILE * fp = fopen(file_name, modes);
		return ((fp == nullptr) ? nullptr : std::shared_ptr<FILE>(fp, fclose));
	};
}

int JobInfo::child_process(const Config & _config) const
{
	struct rlimit max_stack;
	max_stack.rlim_cur = max_stack.rlim_max = (rlim_t) (_config.max_stack.count());
	if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
		LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
		raise(SIGUSR1);
		return -1;
	}

	// set memory limit
	if (_config.max_memory != Config::MEMORY_UNLIMITED) {
		struct rlimit max_memory;
		max_memory.rlim_cur = max_memory.rlim_max = (rlim_t) (_config.max_memory.count()) * 2;
		if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
			LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set cpu time limit (in seconds)
	if (_config.max_cpu_time != Config::TIME_UNLIMITED) {
		struct rlimit max_cpu_time;
		max_cpu_time.rlim_cur = max_cpu_time.rlim_max = (rlim_t) ((_config.max_cpu_time.count() + 1000) / 1000);
		if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
			LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set max process number limit
	if (_config.max_process_number != Config::UNLIMITED) {
		struct rlimit max_process_number;
		max_process_number.rlim_cur = max_process_number.rlim_max = (rlim_t) _config.max_process_number;
		if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
			LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set max output size limit
	if (_config.max_output_size != Config::MEMORY_UNLIMITED) {
		struct rlimit max_output_size;
		max_output_size.rlim_cur = max_output_size.rlim_max = (rlim_t) _config.max_output_size.count();
		if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
			LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	/*
	 * 警告一波！！！！！！！！！！！！！！！！！
	 * 不要使用 shared_ptr 的 reset 方法去重置指针，否则出作用域前不再 p.reset() 一下的话就会资源泄漏
	 * 还有！！！！ shared_ptr 和 unique_ptr 的工作机制不一样！！！！！！！！！！改代码之前一定要查文档 + 做实验！
	 * 学长在这里被坑了很长时间了！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
	 */
	std::shared_ptr<FILE> input_file = nullptr;
	std::shared_ptr<FILE> output_file = nullptr;
	std::shared_ptr<FILE> error_file = nullptr;

	if (_config.input_path != NULL) {
		input_file = open_file(_config.input_path, "r");
		if (!input_file) {
			LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.input_path, "]");
			LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED);
			raise(SIGUSR1);
			return -1;
		}
		// redirect file -> stdin
		// On success, these system calls return the new descriptor.
		// On error, -1 is returned, and errno is set appropriately.
		if (dup2(fileno(input_file.get()), fileno(stdin)) == -1) {
			// todo log
			LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	if (_config.output_path != NULL) {
		output_file = open_file(_config.output_path, "w");
		if (!output_file) {
			LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.output_path, "]");
			LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED);
			raise(SIGUSR1);
			return -1;
		}
		// redirect stdout -> file
		if (dup2(fileno(output_file.get()), fileno(stdout)) == -1) {
			LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	if (_config.error_path != NULL) {
		// if outfile and error_file is the same path, we use the same file pointer
		if (_config.output_path != NULL && strcmp(_config.output_path, _config.error_path) == 0) {
			error_file = output_file;
		} else {
			error_file = open_file(_config.error_path, "w");
			if (!error_file) {
				// todo log
				LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.error_path, "]");
				LOG_FATAL(jobType, sid, log_fp,  RunnerError::DUP2_FAILED);
				raise(SIGUSR1);
				return -1;
			}
		}
		// redirect stderr -> file
		if (dup2(fileno(error_file.get()), fileno(stderr)) == -1) {
			// todo log
			LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set gid
	gid_t group_list[] = { _config.gid };
	if (setgid(_config.gid) == -1 || setgroups(sizeof(group_list) / sizeof(gid_t), group_list) == -1) {
		LOG_FATAL(jobType, sid, log_fp, RunnerError::SETUID_FAILED);
		raise(SIGUSR1);
		return -1;
	}

	// set uid
	if (setuid(_config.uid) == -1) {
		LOG_FATAL(jobType, sid, log_fp, RunnerError::SETUID_FAILED);
		raise(SIGUSR1);
		return -1;
	}

	// load seccomp
	if (_config.seccomp_rule_name != NULL) {
		if (strcmp("c_cpp", _config.seccomp_rule_name) == 0) {
			if (c_cpp_seccomp_rules(_config) != RunnerError::SUCCESS) {
				LOG_FATAL(jobType, sid, log_fp, RunnerError::LOAD_SECCOMP_FAILED);
				raise(SIGUSR1);
				return -1;
			}
		} else if (strcmp("general", _config.seccomp_rule_name) == 0) {
			if (general_seccomp_rules(_config) != RunnerError::SUCCESS) {
				LOG_FATAL(jobType, sid, log_fp, RunnerError::LOAD_SECCOMP_FAILED);
				raise(SIGUSR1);
				return -1;
			}
		} else {		// other rules
			// rule does not exist
			LOG_FATAL(jobType, sid, log_fp, RunnerError::LOAD_SECCOMP_FAILED);
			raise(SIGUSR1);
			return -1;
		}
	}

	auto args = _config.args.getArgs();
	auto env = _config.env.getArgs();
	execve(_config.exe_path, args.get(), env.get());
	/*
	 * 以上三行不可以并作
	 * execve(_config.exe_path, _config.args.getArgs().get(), _config.env.getArgs().get());
	 * 会取到野指针
	 */

	LOG_FATAL(jobType, sid, log_fp, RunnerError::EXECVE_FAILED);
	raise(SIGUSR1);
	return -1;
}

