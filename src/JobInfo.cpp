/*
 * JobInfo.cpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#include "JobInfo.hpp"
#include "global_shared_variable.hpp"
#include "logger.hpp"
#include "compare.hpp"
#include "rules/seccomp_rules.hpp"

#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

#include <cstring>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <wait.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <kerbal/math/randnum.hpp>
using namespace kerbal::math::randnum;

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>


boost::format JobInfo::key_name_tmpl("job_info:%d:%d");
boost::format JobInfo::judge_status_name_tmpl("judge_status:%d:%d");

std::pair<int, int> JobInfo::parser_job_item(std::string job_item)
{
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
		throw std::invalid_argument("invalid job_item arguments: " + job_item);
	}
}

JobInfo::JobInfo(int jobType, int sid) :
		jobType(jobType), sid(sid),

		key_name((key_name_tmpl % jobType % sid).str()),

		judge_status_name((judge_status_name_tmpl % jobType % sid).str())
{
}

JobInfo JobInfo::fetchFromRedis(const Context & conn, int jobType, int sid)
{
	using std::stoi;

	static Operation opt;
	JobInfo res(jobType, sid);
	std::unordered_map<std::string, std::string> query_res;
	try {
		query_res = opt.hgetall(conn, res.key_name);
	} catch (const RedisNilException & e) {
		LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
		throw;
	} catch (const RedisUnexceptedCaseException & e) {
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
		res.pid = stoi(query_res["pid"]);
		res.uid = stoi(query_res["uid"]);
		res.lang = (Language) stoi(query_res["lang"]);
		res.cases = stoi(query_res["cases"]);
		res.cid = stoi(query_res["cid"]);
		res.timeLimit = std::chrono::milliseconds(stoi(query_res["time_limit"]));
		res.memoryLimit = kerbal::utility::MB { stoull(query_res["memory_limit"]) };
		res.postTime = query_res["post_time"];
		res.haveAccepted = stoi(query_res["have_accepted"]);
		res.no_store_ac_code = (bool) stoi(query_res["no_store_ac_code"]);
		res.is_rejudge = (bool) stoi(query_res["is_rejudge"]);
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

//void JobInfo::judge_job(const Context & conn)
//{
//	JobInfo & jobinfo = *this;
//
//	int pid = getpid();
//	int job_id = jobinfo.pid;
//
//	List<std::string> judger(conn, "judger");
//
//	try {
//		int ms = rand_type_between<int>(100, 300);
//		std::chrono::milliseconds judge_consume(ms);
//		std::this_thread::sleep_for(judge_consume);
//
//		static boost::format fmt("job_id:%d   judge_server:%d   judge_pid:%d   judge_consume:%d ms");
//		LOG_INFO(0, job_id, log_fp, fmt % job_id % judge_server_id % pid % judge_consume.count());
//		try {
//			judger.push_back((fmt % job_id % judge_server_id % pid % judge_consume.count()).str());
//		} catch (const std::exception & e) {
//			LOG_FATAL(0, job_id, log_fp, "fail to log judger. Error information: ", e.what());
//			throw;
//		} catch (...) {
//			LOG_FATAL(0, job_id, log_fp, "fail to log judger. Error information: ", "unknow exception");
//			throw;
//		}
//
//	} catch (const std::exception & e) {
//		LOG_FATAL(0, job_id, log_fp, "fail to judge job. Error information: ", e.what());
//		throw;::fdsafds
//	} catch (...) {
//		LOG_FATAL(0, job_id, log_fp, "fail to judge job. Error information: ", "unknow exception");
//		throw;
//	}
//}


void JobInfo::judge_job(const Context & conn)
{
    LOG_DEBUG(jobType, sid, log_fp, "judge start");

    this->change_job_dir();
    this->store_source_code(conn);

    LOG_DEBUG(jobType, sid, log_fp, "store source code finished");

	// compile
    this->commitJudgeStatusToRedis(conn, "status", JudgeStatus::COMPILING);
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
			if (-1 == this->get_compile_info(conn)) {
				LOG_WARNING(jobType, sid, log_fp, "an error occurred while getting compile info");
			}
			break;
	}

	//update update_queue
	static RedisCommand update_queue("rpush update_queue %%d,%%d");
	update_queue.excute(conn, jobType, sid);

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
	this->commitJudgeStatusToRedis(conn, "status", JudgeStatus::RUNNING);
	for (int i = 1; i <= cases; ++i) {
		// change input path
		std::string input_path = (boost::format("%s/%d/in.%d") % input_dir % pid % i).str();
		_config.input_path = input_path.c_str();

		LOG_DEBUG(jobType, sid, log_fp, "running cases ", i);
		Result current_running_result = this->run(_config);
		LOG_DEBUG(jobType, sid, log_fp, "cases ", i, " finished");
		LOG_DEBUG(jobType, sid, log_fp, "cases ", i, " result: ", current_running_result);
		if (current_running_result.result == UnitedJudgeResult::ACCEPTED) {
			// change answer path
			std::string answer = (boost::format("%s/%d/out.%d") % input_dir % pid % i).str();
			this->commitJudgeStatusToRedis(conn, "status", JudgeStatus::JUDGING);
			// compare
			LOG_DEBUG(jobType, sid, log_fp, "comparing ", i);
			UnitedJudgeResult compare_result = compare(answer.c_str(), _config.output_path);
			LOG_DEBUG(jobType, sid, log_fp, "case ", i, " compare result: ", compare_result);
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

void JobInfo::commitJudgeStatusToRedis(const Context & conn, const std::string & key, JudgeStatus status)
{
	static Operation opt;
	try {
		opt.hset(conn, judge_status_name, key, (int) status);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "commit judge status failed ", e.what());
		throw;
	}
}

void JobInfo::commitJudgeResultToRedis(const Context & conn, const Result & result) const
{
	static Operation opt;
	try {
		opt.hmset(conn, judge_status_name,
				"status", (int) JudgeStatus::FINISHED,
				"result", (int) result.result,
				"cpu_time", result.cpu_time.count(),
				"real_time", result.real_time.count(),
				"memory", (kerbal::utility::Byte(result.memory)).count());
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "commit judge result failed ", e.what());
		throw;
	}
}

void JobInfo::set_compile_info(const Context & conn, const char *compile_info)
{
	try {
		RedisCommand cmd("set compile_info:%%d:%%d %%s");
		cmd.excute(conn, jobType, sid, compile_info);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "set compile info failed ", e.what());
		throw;
	}
}

int JobInfo::get_compile_info(const Context & conn)
{
// 获取编译错误信息

	FILE *fp = fopen("compiler.out", "rb");
	if (!fp) {
		LOG_FATAL(jobType, sid, log_fp, "cannot open compiler.out");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long f_size = ftell(fp);
	rewind(fp);
	if (f_size > MYSQL_TEXT_MAX_SIZE) {
		LOG_FATAL(jobType, sid, log_fp, "compile info too long");
		fclose(fp);
		return -1;
	}
	char *buffer = (char *) malloc(f_size * sizeof(char));
	if (!buffer) {
		LOG_FATAL(jobType, sid, log_fp, "memory allocate error");
		fclose(fp);
		return -1;
	}
	long fread_result = fread(buffer, 1, f_size, fp);
	if (fread_result != f_size) {
		LOG_FATAL(jobType, sid, log_fp, "read compile info error");
		fclose(fp);
		free(buffer);
		return -1;
	}
	this->set_compile_info(conn, buffer);
	free(buffer);
	fclose(fp);
	return 0;
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
		default:
			throw CalculateSimilarityException("language unknown");
	}

	int sim_cmd_return_value = system(sim_cmd.c_str());
	if (-1 == sim_cmd_return_value) {
		throw CalculateSimilarityException("excute failed while calculating similarity");
	}
	if (!WIFEXITED(sim_cmd_return_value) || WEXITSTATUS(sim_cmd_return_value)) {
		throw CalculateSimilarityException("sim command exits incorrectly, exit status: " + std::to_string(WEXITSTATUS(sim_cmd_return_value)));
	}

	std::ifstream fp("sim.txt", std::ios::in);

	if (!fp) {
		throw CalculateSimilarityException("an error occurred while opening sim.txt");
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
			std::string::reverse_iterator be = std::find_if_not(en, buf.rend(), [](char c) {
				return '0' <= c && c <= '9';
			});
			--en;
			--be;
			try {
				int now_similarity = std::stoi(buf.substr(buf.length() - (std::distance(buf.rbegin(), be)) - 1, std::distance(en, be)));
				max_similarity = std::max(max_similarity, now_similarity);
			} catch (const std::invalid_argument & e) {
				throw CalculateSimilarityException("an error occurred while calculate max similarity");
			}
		}
	}

	return max_similarity;
}


int JobInfo::store_code_to_accepted_solutions_dir() const
{
// 将已经AC且no_store_ac_code标志不为1的代码存入accepted_solutions_dir目录
	std::string code_path = accepted_solutions_dir;

	// 以下逐个检查文件夹可写权限
	if (access(code_path.c_str(), W_OK) != 0) {
		LOG_FATAL(jobType, sid, log_fp, "accepted_solutions_dir does not have write permission");
		return -1;
	}
	code_path += "/" + std::to_string(pid);
	if (access(code_path.c_str(), W_OK) != 0) {
		if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "cannot create directory ", code_path);
			return -1;
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
		default:
			LOG_FATAL(jobType, sid, log_fp, "language unknown");
			return -1;
	}
	if (access(code_path.c_str(), W_OK) != 0) {
		if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "cannot create directory ", code_path);
			return -1;
		}
	}

	// 检查本题已经保存的AC代码份数是否超过了stored_ac_code_max_num
	DIR *dir = opendir(code_path.c_str());
	if (dir == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "cannot open directory ", code_path, " for checking the number of accepted code");
		return -1;
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
		LOG_FATAL(jobType, sid, log_fp, "fork failed while copying code to accepted_solutions_dir");
		return -1;
	}
	if (!WIFEXITED(copy_cmd_return_value) || WEXITSTATUS(copy_cmd_return_value)) {
		LOG_FATAL(jobType, sid, log_fp, "an error occurred while copying code to accepted_solutions_dir, exit status: ", WEXITSTATUS(copy_cmd_return_value));
		return -1;
	}
	return 0;
}

void JobInfo::change_job_dir()
{
	static boost::format dir_templ(init_dir + "/job-%d-%d");
	dir = (dir_templ % jobType % sid).str();
	// TODO
	LOG_DEBUG(jobType, sid, log_fp, "make dir: ", dir);
	mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	chmod(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	if (chdir(dir.c_str())) {
		LOG_FATAL(jobType, sid, log_fp, "can not change job dir: [", dir, "]");
		exit(-1);
	}
}

void JobInfo::clean_job_dir() const
{
	LOG_DEBUG(jobType, sid, log_fp, "clean dir failed.");
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
		exit(-1);
	}
	rmdir(dir.c_str());
}

void JobInfo::push_back_failed_judge_job(const Context & conn, List<std::string> & judge_failer_list) const
{
	LOG_WARNING(jobType, sid, log_fp, "push back to judge failed list");
	Result result;
	result.result = UnitedJudgeResult::SYSTEM_ERROR;
	this->commitJudgeResultToRedis(conn, result);
	try {
		judge_failer_list.push_back(std::to_string(jobType) + ":" + std::to_string(sid));
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: ", e.what());
	} catch (...) {
		LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: ", "unknown exception");
	}
}

std::chrono::milliseconds timevalToChrono(const timeval & val)
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(seconds(val.tv_sec) + microseconds(val.tv_usec));
}

Result JobInfo::run(const Config & _config) const noexcept
{
	using namespace std::chrono;

	Result _result;

	// check whether current user is root
	uid_t uid = getuid();
	if (uid != 0) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", "ROOT_REQUIRED");
		_result.setErrorCode(RunnerError::ROOT_REQUIRED);
		return _result;
	}

	// check args
	if (_config.check_is_valid_config() == false) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", "INVALID_CONFIG");
		_result.setErrorCode(RunnerError::INVALID_CONFIG);
		return _result;
	}

	// record current time
	struct timeval start, end;
	gettimeofday(&start, NULL);

	pid_t child_pid = fork();

	// pid < 0 shows clone failed
	if (child_pid < 0) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", "FORK_FAILED");
		_result.setErrorCode(RunnerError::FORK_FAILED);
		return _result;
	} else if (child_pid == 0) {
		this->child_process(_config);
	} else if (child_pid > 0) {
		// create new thread to monitor process running time
		if (_config.max_real_time != Config::TIME_UNLIMITED) {
			try {
				std::thread timeout_killer_thread([](pid_t child_pid, milliseconds timeout) {
					static constexpr seconds one_second {1};
					std::this_thread::sleep_for (timeout + one_second);
					if (kill(child_pid, SIGKILL) != 0) {
						return;
					}
					return;
				}, child_pid, _config.max_real_time);
				try {
					timeout_killer_thread.detach();
				} catch (const std::system_error & e) {
					kill(pid, SIGKILL);
					LOG_FATAL(jobType, sid, log_fp, "Error: ", "PTHREAD_FAILED, ", e.what());
					_result.setErrorCode(RunnerError::PTHREAD_FAILED);
					return _result;
				}
			} catch (const std::system_error & e) {
				kill(child_pid, SIGKILL);
				LOG_FATAL(jobType, sid, log_fp, "Error: ", "PTHREAD_FAILED, ", e.what());
				_result.setErrorCode(RunnerError::PTHREAD_FAILED);
				return _result;
			} catch (...) {
				kill(child_pid, SIGKILL);
				LOG_FATAL(jobType, sid, log_fp, "Error: ", "PTHREAD_FAILED, ", "unknown exception");
				_result.setErrorCode(RunnerError::PTHREAD_FAILED);
				return _result;
			}
			LOG_DEBUG(jobType, sid, log_fp, "timeout thread success");
		}

		int status;
		struct rusage resource_usage;

		// wait for child process to terminate
		// on success, returns the process ID of the child whose state has changed;
		// On error, -1 is returned.
		if (wait4(child_pid, &status, WSTOPPED, &resource_usage) == -1) {
			kill(child_pid, SIGKILL);
			LOG_FATAL(jobType, sid, log_fp, "Error: ", "WAIT_FAILED");
			_result.setErrorCode(RunnerError::WAIT_FAILED);
			return _result;
		}

		_result.exit_code = WEXITSTATUS(status);
		_result.cpu_time = timevalToChrono(resource_usage.ru_utime) + timevalToChrono(resource_usage.ru_stime);
		_result.memory = kerbal::utility::KB(resource_usage.ru_maxrss);

		// get end time
		gettimeofday(&end, NULL);
		_result.real_time = timevalToChrono(end) - timevalToChrono(start);

		if (_result.exit_code != 0) {
			_result.result = UnitedJudgeResult::RUNTIME_ERROR;
			LOG_FATAL(jobType, sid, log_fp, "exit code != 0");
			return _result;
		}
		// if signaled
		if (WIFSIGNALED(status) != 0) {
			_result.signal = WTERMSIG(status);
			LOG_DEBUG(jobType, sid, log_fp, "signal: ", _result.signal);
			switch (_result.signal) {
				case SIGSEGV:
					if (_config.max_memory != Config::MEMORY_UNLIMITED && _result.memory > _config.max_memory) {
						_result.result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
					} else {
						_result.result = UnitedJudgeResult::RUNTIME_ERROR;
					}
					break;
				case SIGUSR1:
					_result.result = UnitedJudgeResult::SYSTEM_ERROR;
					break;
				default:
					_result.result = UnitedJudgeResult::RUNTIME_ERROR;
			}
		} else {
			if (_config.max_memory != Config::MEMORY_UNLIMITED && _result.memory > _config.max_memory) {
				_result.result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
			}
		}
		if (_config.max_real_time != Config::TIME_UNLIMITED && _result.real_time > _config.max_real_time) {
			_result.result = UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED;
		}
		if (_config.max_cpu_time != Config::TIME_UNLIMITED && _result.cpu_time > _config.max_cpu_time) {
			_result.result = UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED;
		}
		return _result;
	}
}

void JobInfo::store_source_code(const Context & conn) const
{
	static RedisCommand cmd("hget source_code:%%d:%%d source");
	AutoFreeReply reply = cmd.excute(conn, this->jobType, this->sid);
	std::string file_name;
	switch (this->lang) {
		case Language::C:
			file_name = "Main.c";
			break;
		case Language::Cpp:
		case Language::Cpp14:
			file_name = "Main.cpp";
			break;
		case Language::Java:
			file_name = "Main.java";
			break;
	}
	std::ofstream fout(file_name, std::ios::out);
	if (!fout) {
		LOG_FATAL(jobType, sid, log_fp, "store source code failed");
		return;
	}
	fout << reply->str;
}

Result JobInfo::compile() const
{
	Result result = this->run(Config(*this, Config::COMPILE));

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
			LOG_FATAL(jobType, sid, log_fp, "system error occured while compiling: ", result);
			return result;
		default:
			LOG_FATAL(jobType, sid, log_fp, "unexpected compile result: ", result);
			result.result = UnitedJudgeResult::COMPILE_ERROR;
			return result;
	}
}



void close_file(FILE *fp, ...) {
    va_list args;
    va_start(args, fp);

    if (fp != NULL) {
        fclose(fp);
    }

    va_end(args);
}

int JobInfo::child_process(const Config & _config) const
{
	FILE *input_file = NULL, *output_file = NULL, *error_file = NULL;

	struct rlimit max_stack;
	max_stack.rlim_cur = max_stack.rlim_max = (rlim_t) (_config.max_stack.count());
	if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETRLIMIT_FAILED);
		close_file(input_file, output_file, error_file);
		raise(SIGUSR1);
		return -1;
	}

	// set memory limit
	if (_config.max_memory != Config::MEMORY_UNLIMITED) {
		struct rlimit max_memory;
		max_memory.rlim_cur = max_memory.rlim_max = (rlim_t) (_config.max_memory.count()) * 2;
		if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETRLIMIT_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set cpu time limit (in seconds)
	if (_config.max_cpu_time != Config::TIME_UNLIMITED) {
		struct rlimit max_cpu_time;
		max_cpu_time.rlim_cur = max_cpu_time.rlim_max = (rlim_t) ((_config.max_cpu_time.count() + 1000) / 1000);
		if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETRLIMIT_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set max process number limit
	if (_config.max_process_number != Config::UNLIMITED) {
		struct rlimit max_process_number;
		max_process_number.rlim_cur = max_process_number.rlim_max = (rlim_t) _config.max_process_number;
		if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETRLIMIT_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set max output size limit
	if (_config.max_output_size != Config::MEMORY_UNLIMITED) {
		struct rlimit max_output_size;
		max_output_size.rlim_cur = max_output_size.rlim_max = (rlim_t) _config.max_output_size.count();
		if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETRLIMIT_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	if (_config.input_path != NULL) {
		input_file = fopen(_config.input_path, "r");
		if (input_file == NULL) {
			LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.input_path, "]");
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
		// redirect file -> stdin
		// On success, these system calls return the new descriptor.
		// On error, -1 is returned, and errno is set appropriately.
		if (dup2(fileno(input_file), fileno(stdin)) == -1) {
			// todo log
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	if (_config.output_path != NULL) {
		output_file = fopen(_config.output_path, "w");
		if (output_file == NULL) {
			LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.output_path, "]");
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
		// redirect stdout -> file
		if (dup2(fileno(output_file), fileno(stdout)) == -1) {
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	if (_config.error_path != NULL) {
		// if outfile and error_file is the same path, we use the same file pointer
		if (_config.output_path != NULL && strcmp(_config.output_path, _config.error_path) == 0) {
			error_file = output_file;
		} else {
			error_file = fopen(_config.error_path, "w");
			if (error_file == NULL) {
				// todo log
				LOG_FATAL(jobType, sid, log_fp, "can not open [", _config.error_path, "]");
				LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
				close_file(input_file, output_file, error_file);
				raise(SIGUSR1);
				return -1;
			}
		}
		// redirect stderr -> file
		if (dup2(fileno(error_file), fileno(stderr)) == -1) {
			// todo log
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::DUP2_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	// set gid
	gid_t group_list[] = { _config.gid };
	if (_config.gid != -1 && (setgid(_config.gid) == -1 || setgroups(sizeof(group_list) / sizeof(gid_t), group_list) == -1)) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETUID_FAILED);
		close_file(input_file, output_file, error_file);
		raise(SIGUSR1);
		return -1;
	}

	// set uid
	if (_config.uid != -1 && setuid(_config.uid) == -1) {
		LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::SETUID_FAILED);
		close_file(input_file, output_file, error_file);
		raise(SIGUSR1);
		return -1;
	}

	// load seccomp
	if (_config.seccomp_rule_name != NULL) {
		if (strcmp("c_cpp", _config.seccomp_rule_name) == 0) {
			if (c_cpp_seccomp_rules(_config) != RunnerError::SUCCESS) {
				LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::LOAD_SECCOMP_FAILED);
				close_file(input_file, output_file, error_file);
				raise(SIGUSR1);
				return -1;
			}
		} else if (strcmp("general", _config.seccomp_rule_name) == 0) {
			if (general_seccomp_rules(_config) != RunnerError::SUCCESS) {
				LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::LOAD_SECCOMP_FAILED);
				close_file(input_file, output_file, error_file);
				raise(SIGUSR1);
				return -1;
			}
		} else {		// other rules
			// rule does not exist
			LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::LOAD_SECCOMP_FAILED);
			close_file(input_file, output_file, error_file);
			raise(SIGUSR1);
			return -1;
		}
	}

	auto args = _config.args.getArgs();
	auto env = _config.env.getArgs();

	execve(_config.exe_path, args.get(), env.get());

	LOG_FATAL(jobType, sid, log_fp, "Error: ", RunnerError::EXECVE_FAILED);
	close_file(input_file, output_file, error_file);
	raise(SIGUSR1);
	return -1;
	return 0;
}

