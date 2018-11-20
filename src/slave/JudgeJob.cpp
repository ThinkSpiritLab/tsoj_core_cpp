/*
 * JobInfo.cpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#include "JudgeJob.hpp"

#include <thread>
#include <algorithm>
#include <cstring>
#include <functional>

#include <dirent.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "logger.hpp"
#include "process.hpp"
#include "compare.hpp"
#include "Config.hpp"
#include "global_shared_variable.hpp"
#include "boost_format_suffix.hpp"

#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_type_cast.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>

using namespace kerbal::redis;
using kerbal::redis::RedisContext;


JudgeJob::JudgeJob(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & conn) :
			supper_t(jobType, s_id, conn), dir((boost::format(init_dir + "/job-%d-%d") % jobType % s_id).str())
{
	kerbal::redis::Operation opt(redisConn);
	static boost::format key_name_tmpl("job_info:%d:%d");
	const std::string job_info_key = (key_name_tmpl % jobType % s_id).str();

	try {
		this->have_accepted = (bool)(opt.hget<int>(job_info_key, "have_accepted"));
		this->no_store_ac_code = (bool)(opt.hget<int>(job_info_key, "no_store_ac_code"));
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Fail to fetch job details.", e);
		throw;
	}
}

void JudgeJob::handle()
{
	LOG_DEBUG(jobType, s_id, log_fp, "JudgeJob::handle");

	this->change_job_dir();
	this->storeSourceCode();

	LOG_DEBUG(jobType, s_id, log_fp, "store source code finished");

	int similarity_percentage = -1;
	try {
		similarity_percentage = this->calculate_similarity();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "An error occurred while calculate similarity.", e);
		//DO NOT THROW
	}

	LOG_DEBUG(jobType, s_id, log_fp, "similarity: ", similarity_percentage);

	if (this->commit_similarity_details_to_redis() == false) {
		LOG_FATAL(jobType, s_id, log_fp, "An error occurred while commit similarity details to redis.");
	}

	SolutionDetails judgeResult;
	if (this->similarity_threshold != 0 && !this->have_accepted &&
			similarity_percentage > this->similarity_threshold) {
		judgeResult.judge_result = UnitedJudgeResult::SIMILAR_CODE;
	} else {

		// compile
		this->commitJudgeStatusToRedis(JudgeStatus::COMPILING);
		LOG_DEBUG(jobType, s_id, log_fp, "compile start");
		judgeResult = static_cast<SolutionDetails>(this->compile());
		LOG_DEBUG(jobType, s_id, log_fp, "compile finished. compile result: ", judgeResult);

		switch (judgeResult.judge_result) {
			case UnitedJudgeResult::ACCEPTED: {
				LOG_DEBUG(jobType, s_id, log_fp, "compile success");
				// 编译成功则 run
				judgeResult = static_cast<SolutionDetails>(this->running());
				LOG_DEBUG(jobType, s_id, log_fp, "judge finished");

				// 如果用户 AC 且要求留存代码, 则将代码保存至留存目录
				if (judgeResult.judge_result == UnitedJudgeResult::ACCEPTED  && this->no_store_ac_code != true) {
					this->storeSourceCode("%s/%d/%d/"_fmt(accepted_solutions_dir, p_id, lang == Language::Java ? 2 : 0).str(), std::to_string(s_id.to_literal()));
				}
				break;
			}
			case UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED:
				LOG_WARNING(jobType, s_id, log_fp, "compile resource limit exceed");
				break;
			case UnitedJudgeResult::SYSTEM_ERROR:
				LOG_FATAL(jobType, s_id, log_fp, "system error");
				break;
			case UnitedJudgeResult::COMPILE_ERROR:
			default:
				LOG_DEBUG(jobType, s_id, log_fp, "compile failed");

				if (this->set_compile_info() == false) {
					LOG_WARNING(jobType, s_id, log_fp, "An error occurred while getting compile info.");
				}
				break;
		}
	}

	LOG_DEBUG(jobType, s_id, log_fp, "judge result: ", judgeResult);
	this->commitJudgeResultToRedis(judgeResult, similarity_percentage);

	//update update_queue
	RedisCommand update_queue("rpush update_queue %d,%d");
	update_queue.execute(redisConn, jobType, s_id);

#ifndef DEBUG
	// clear the dir
	this->clean_job_dir();
#endif

}

int JudgeJob::calculate_similarity()
{
	std::string sim_cmd;
	switch (this->lang) {
		case Language::C:
			sim_cmd = (boost::format("sim_c -S -P Main.c / %s/%d/0/* > sim.txt") % accepted_solutions_dir % this->p_id).str();
			break;
		case Language::Cpp:
		case Language::Cpp14:
			sim_cmd = (boost::format("sim_c -S -P Main.cpp / %s/%d/0/* > sim.txt") % accepted_solutions_dir % this->p_id).str();
			break;
		case Language::Java:
			sim_cmd = (boost::format("sim_java -S -P Main.java / %s/%d/2/* > sim.txt") % accepted_solutions_dir % this->p_id).str();
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

Result JudgeJob::running()
{
	// 构建运行配置（但暂未使用）
	RunningConfig config(*this);

	Result result;
	this->commitJudgeStatusToRedis(JudgeStatus::RUNNING);
	for (int i = 1; i <= cases; ++i) {
		// change input path
		std::string input_path = "%s/%d/in.%d"_fmt(input_dir, p_id, i).str();
		config.input_path = input_path.c_str();

		// 根据 config 中的配置执行一组测试并得到结果
		Result current_running_result = this->execute(config);
		LOG_DEBUG(jobType, s_id, log_fp, "running case: ", i, " finished; result: ", current_running_result);

		if (current_running_result.judge_result != UnitedJudgeResult::ACCEPTED) {
			return current_running_result;
		}

		// change answer path
		std::string answer = "%s/%d/out.%d"_fmt(input_dir, p_id, i).str();
		this->commitJudgeStatusToRedis(JudgeStatus::JUDGING);

		// compare
		UnitedJudgeResult compare_result = compare(answer.c_str(), config.output_path);
		LOG_DEBUG(jobType, s_id, log_fp, "comparing case: ", i, " finished; compare result: ", compare_result);

		if (compare_result != UnitedJudgeResult::ACCEPTED) {
			current_running_result.judge_result = compare_result;
			return current_running_result;
		}

		result = current_running_result;

		//set_judge_status(conn, type, job_id, "cases_finished", i);
	}
	LOG_DEBUG(jobType, s_id, log_fp, "all cases accepted");
	return result;
}

/*
 * 将评测结果提交到 redis 数据库。JudgeJob::commitJudgeResultToRedis 实际上是调用了此函数。之所以将此函数分离出来，
 * 是因为需要配合静态成员函数 insert_into_failed，在类未能成功实例化的时候使用。而此处匿名空间则保证了，只能由此 cpp 文件的
 * 成员函数调用此方法，杜绝了其余文件内函数调用该函数的可能，保证了安全。
 */	
namespace
{
	void
	staticCommitJudgeResultToRedis(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext redisConn, const SolutionDetails & result, int similarity_percentage) try
	{
		static boost::format judge_status_name_tmpl("judge_status:%d:%d");
		Operation opt(redisConn);
			opt.hmset((judge_status_name_tmpl % jobType % s_id).str(),
					  "status", (int) JudgeStatus::FINISHED,
					  "result", (int) result.judge_result,
					  "cpu_time", result.cpu_time.count(),
					  "real_time", result.real_time.count(),
					  "memory", (kerbal::utility::Byte(result.memory)).count(),
					  "similarity_percentage", similarity_percentage,
					  "judge_server_id", judge_server_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Commit judge result failed.", e, "; solution details: ", result);
		throw;
	}
}

void JudgeJob::commitJudgeResultToRedis(const SolutionDetails & result, int similarity_percentage)
{
	staticCommitJudgeResultToRedis(jobType, s_id, redisConn, result, similarity_percentage);
}

bool JudgeJob::commit_similarity_details_to_redis() noexcept try{
	// open sim.txt
	std::ifstream fin("sim.txt", std::ios::in);
	if (!fin) {
		throw JobHandleException("sim.txt open failed");
	}

	/* 
	* read
	* Tips: redis 的 string 类型的值最大能存储 512 MB， 显然如果不加以限制，一下占用这么大的资源,
	* 是不合理的。考虑到以后可能需要 update 到 mysql， 此处与 mysql text 的最大长度保持一致。
	* 中间有些与 set_compile_info() 相同的部分，可以考虑再抽一个函数出来?
	*/
	constexpr int MAX_SIZE = 65535;
	char buffer[MAX_SIZE  - 1+ 10];
	fin.read(buffer, MAX_SIZE - 1);

	if (fin.bad()) {
		LOG_FATAL(jobType, s_id, log_fp, "Read sim.txt error, only ", fin.gcount(), " could be read");
		LOG_FATAL(jobType, s_id, log_fp, "The read buffer: ", buffer);
		throw JobHandleException("sim.txt read failed");
	}

	int length_read = fin.gcount();
	buffer[length_read] = '\0';

	//TODO 2018-10-20 trouble maker!
	// Mysql 中存储字符长度有限，此处似乎超出长度可能导致的乱码问题未解决
//	if (MAX_SIZE >= 3 && length > MAX_SIZE) {
//		char * end = buffer + MAX_SIZE - 3;
//		for (int i = 0; i < 3; ++i) {
//			*end = '.';
//			++end;
//		}
//	}

	// commit
	try {
		static boost::format simtxt_name_tmpl("similarity_details:%d:%d");
		Operation opt(redisConn);
		opt.set((simtxt_name_tmpl % jobType % s_id).str(), buffer);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Set similarity details failed.", e);
		throw;
	}

	return true;
} catch (const std::exception & e) {
	EXCEPT_FATAL(jobType, s_id, log_fp, "An error occurred while commit similarity details to redis.", e);
	// 查重失败并非致命错误， 若是查重出差，不应当影响正常判题结果。
	return false;
}

bool JudgeJob::set_compile_info() noexcept
{
	std::ifstream fin("compiler.out", std::ios::in);
	if (!fin) {
		LOG_FATAL(jobType, s_id, log_fp, "Cannot open compiler.out");
		return false;
	}

	constexpr int MYSQL_TEXT_MAX_SIZE = 65535;
	char buffer[MYSQL_TEXT_MAX_SIZE - 1 + 10];
	fin.read(buffer, MYSQL_TEXT_MAX_SIZE - 1);

	if (fin.bad()) {
		LOG_FATAL(jobType, s_id, log_fp, "Read compile info error, only ", fin.gcount(), " could be read");
		LOG_FATAL(jobType, s_id, log_fp, "The read buffer: ", buffer);
		return false;
	}

	int length_read = fin.gcount();
	buffer[length_read] = '\0';

	//TODO 2018-10-20 trouble maker!
	// Mysql 中存储字符长度有限，此处似乎超出长度可能导致的乱码问题未解决
//	if (MYSQL_TEXT_MAX_SIZE >= 3 && length_read > MYSQL_TEXT_MAX_SIZE) {
//		char * end = buffer + MYSQL_TEXT_MAX_SIZE - 3;
//		for (int i = 0; i < 3; ++i) {
//			*end = '.';
//			++end;
//		}
//	}

	try {
		static boost::format compile_info("compile_info:%d:%d");
		Operation opt(redisConn);
		opt.set((compile_info % jobType % s_id).str(), buffer);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Set compile info failed.", e);
		return false;
	}

	return true;
}

void JudgeJob::change_job_dir() const
{
	// TODO
	LOG_DEBUG(jobType, s_id, log_fp, "make dir: ", dir);
	mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	chmod(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	if (chdir(dir.c_str())) {
		LOG_FATAL(jobType, s_id, log_fp, "can not change to job dir: [", dir, "]");
		throw JobHandleException("can not change to job dir");
	}
}

void JudgeJob::clean_job_dir() const noexcept try
{
	LOG_DEBUG(jobType, s_id, log_fp, "clean dir");
	std::unique_ptr<DIR, int (*)(DIR *)> dp(opendir("."), closedir);
	if (dp == nullptr) {
		LOG_FATAL(jobType, s_id, log_fp, "clean dir failed.");
		return;
	}

	for (const dirent * entry = nullptr; (entry = readdir(dp.get())) != nullptr;) {
		if (std::strcmp(".", entry->d_name) != 0 && std::strcmp("..", entry->d_name) != 0) {
			unlink(entry->d_name);
		}
	}
	if (chdir("..") != 0) {
		LOG_FATAL(jobType, s_id, log_fp, "can not go back to ../");
		return;
	}
	rmdir(dir.c_str());
	return;
} catch (...) {
	return;
}

namespace
{
	std::chrono::milliseconds timevalToChrono(const timeval & val)
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(seconds(val.tv_sec) + microseconds(val.tv_usec));
	}
}


Result JudgeJob::execute(const Config & config) const noexcept
{
	using namespace std::chrono;
	using namespace kerbal::compatibility::chrono_suffix;

	Result result;

	// check whether current user is root
	if (getuid() != 0) {
		LOG_FATAL(jobType, s_id, log_fp, "ROOT_REQUIRED");
		result.setErrorCode(RunnerError::ROOT_REQUIRED);
		return result;
	}

	// check args
	if (config.check_is_valid_config() == false) {
		LOG_FATAL(jobType, s_id, log_fp, "INVALID_CONFIG");
		result.setErrorCode(RunnerError::INVALID_CONFIG);
		return result;
	}

	process child_process;

	// 此处分离了一个运行子进程，稍后该进程内自我完成工作配置，然后替换为用户编写的程序，用于实际的编译/运行
	try {
		child_process = process(std::bind(&JudgeJob::child_process, this, std::cref(config))); // equals to: this->child_process(config);
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, s_id, log_fp, "Fork failed. Error information: ", e.what());
		result.setErrorCode(RunnerError::FORK_FAILED);
		return result;
	}

	// record current time
	time_point<high_resolution_clock> start(high_resolution_clock::now());

	if (config.limitRealTime()) {
		std::thread timeout_killer_thread;
		try {
			timeout_killer_thread = std::thread([&child_process](milliseconds timeout) {
				std::this_thread::sleep_for(timeout + 1_s);
				if (child_process.kill(SIGKILL) != 0) {
					//TODO log
				}
			}, config.max_real_time);
		} catch (const std::system_error & e) {
			child_process.kill(SIGKILL);
			EXCEPT_FATAL(jobType, s_id, log_fp, "Thread construct failed.", e, " , error code: ", e.code().value());
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		} catch (...) {
			child_process.kill(SIGKILL);
			UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Thread construct failed.");
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		}

		try {
			timeout_killer_thread.detach();
		} catch (const std::system_error & e) {
			child_process.kill(SIGKILL);
			EXCEPT_FATAL(jobType, s_id, log_fp, "Thread detach failed.", e, " , error code: ", e.code().value());
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		} catch (...) {
			child_process.kill(SIGKILL);
			UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Thread detach failed.");
			result.setErrorCode(RunnerError::PTHREAD_FAILED);
			return result;
		}
		LOG_DEBUG(jobType, s_id, log_fp, "Timeout thread work success");
	}

	int status;
	struct rusage resource_usage;

	// wait for child process to terminate
	// on success, returns the process ID of the child whose state has changed;
	// On error, -1 is returned.
	if (child_process.join(&status, WSTOPPED, &resource_usage) == -1) {
		child_process.kill(SIGKILL);
		LOG_FATAL(jobType, s_id, log_fp, "WAIT_FAILED");
		result.setErrorCode(RunnerError::WAIT_FAILED);
		return result;
	}

	result.real_time = duration_cast<milliseconds>(high_resolution_clock::now() - start);
	result.cpu_time = timevalToChrono(resource_usage.ru_utime) + timevalToChrono(resource_usage.ru_stime);
	result.memory = kerbal::utility::KB(resource_usage.ru_maxrss);
	result.exit_code = WEXITSTATUS(status);

	if (result.exit_code != 0) {
		result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
		return result;
	}
	// if signaled
	if (WIFSIGNALED(status) != 0) {
		result.signal = WTERMSIG(status);
		LOG_DEBUG(jobType, s_id, log_fp, "signal: ", result.signal);
		switch (result.signal) {
			case SIGSEGV:
				if (config.limitMemory() && result.memory > config.max_memory) {
					result.judge_result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
				} else {
					result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
				}
				break;
			case SIGUSR1:
				result.judge_result = UnitedJudgeResult::SYSTEM_ERROR;
				break;
			default:
				result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
		}
	} else {
		if (config.limitMemory() && result.memory > config.max_memory) {
			result.judge_result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
		}
	}
	if (config.limitRealTime() && result.real_time > config.max_real_time) {
		result.judge_result = UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED;
	}
	if (config.limitCPUTime() && result.cpu_time > config.max_cpu_time) {
		result.judge_result = UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED;
	}
	return result;
}

Result JudgeJob::compile() const noexcept
{
	/*
	 * 与 running 中不同，运行测试样例可能需要多次，而编译只需执行一次，因此只需要生成一个匿名的 CompileConfig ，节约了传递成本
	 * （也好看
	 */
	Result result = this->execute(CompileConfig(*this));

	switch (result.judge_result) {
		case UnitedJudgeResult::ACCEPTED:
			return result;
		case UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED:
		case UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED:
		case UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED:
			result.judge_result = UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED;
			return result;
		case UnitedJudgeResult::RUNTIME_ERROR:
			result.judge_result = UnitedJudgeResult::COMPILE_ERROR;
			return result;
		case UnitedJudgeResult::SYSTEM_ERROR:
			LOG_FATAL(jobType, s_id, log_fp, "System error occurred while compiling. Execute result: ", result);
			return result;
		default:
			LOG_FATAL(jobType, s_id, log_fp, "Unexpected compile result: ", result);
			result.judge_result = UnitedJudgeResult::COMPILE_ERROR;
			return result;
	}
}

bool JudgeJob::child_process(const Config & config) const
{
	struct rlimit max_stack;
	max_stack.rlim_cur = max_stack.rlim_max = static_cast<rlim_t>(config.max_stack.count());
	if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
		LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETRLIMIT_FAILED);
		raise(SIGUSR1);
		return false;
	}

	// set memory limit
	if (config.limitMemory()) {
		struct rlimit max_memory;
		max_memory.rlim_cur = max_memory.rlim_max = static_cast<rlim_t>(kerbal::utility::Byte { config.max_memory }.count() * 2);
		if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return false;
		}
	}

	// set cpu time limit (in seconds)
	if (config.limitCPUTime()) {
		struct rlimit max_cpu_time;
		max_cpu_time.rlim_cur = max_cpu_time.rlim_max = static_cast<rlim_t>((std::chrono::milliseconds { config.max_cpu_time }.count() + 1000) / 1000);
		if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return false;
		}
	}

	// set max process number limit
	if (config.limitProcessNumber()) {
		struct rlimit max_process_number;
		max_process_number.rlim_cur = max_process_number.rlim_max = static_cast<rlim_t>(config.max_process_number);
		if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return false;
		}
	}

	// set max output size limit
	if (config.limitOutput()) {
		struct rlimit max_output_size;
		max_output_size.rlim_cur = max_output_size.rlim_max = static_cast<rlim_t>(kerbal::utility::Byte { config.max_output_size }.count());
		if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETRLIMIT_FAILED);
			raise(SIGUSR1);
			return false;
		}
	}

	std::unique_ptr<FILE, int (*)(FILE *)> input_file(fopen(config.input_path, "r"), fclose);
	if (config.input_path != nullptr) {
		if (!input_file) {
			LOG_FATAL(jobType, s_id, log_fp, "can not open [", config.input_path, "]");
			raise(SIGUSR1);
			return false;
		}
		// redirect file -> stdin
		// On success, these system calls return the new descriptor.
		// On error, -1 is returned, and errno is set appropriately.
		if (dup2(fileno(input_file.get()), fileno(stdin)) == -1) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::DUP2_FAILED, "dup2 input file failed");
			raise(SIGUSR1);
			return false;
		}
	}

	/*
	 *  Out      |  Err
	 *  -------------------------
	 *  null     |  null
	 *  null     |  err.txt
	 *  out.txt  |  null
	 *  out.txt  |  out.txt (DO NOT open twice)
	 *  out.txt  |  err.txt
	 */
	std::shared_ptr<FILE> output_file = nullptr, error_file = nullptr;

	if (config.output_path != nullptr && config.error_path != nullptr &&
		strcmp(config.output_path, config.error_path) == 0) {
		// 标准输出与错误输出指定为同一个文件
		auto of_ptr = fopen(config.output_path, "w"); //DO NOT fclose this ptr while exit brace
		if (!of_ptr) {
			LOG_FATAL(jobType, s_id, log_fp, "can not open [", config.output_path, "]");
			raise(SIGUSR1);
			return false;
		} else {
			output_file.reset(of_ptr, fclose);
			error_file = output_file;
		}

	} else {
		if (config.output_path != nullptr) {
			auto of_ptr = fopen(config.output_path, "w"); //DO NOT fclose this ptr while exit brace
			if (!of_ptr) {
				LOG_FATAL(jobType, s_id, log_fp, "can not open [", config.output_path, "]");
				raise(SIGUSR1);
				return false;
			} else {
				output_file.reset(of_ptr, fclose);
			}
		}

		if (config.error_path != nullptr) {
			auto ef_ptr = fopen(config.error_path, "w"); //DO NOT fclose this ptr while exit brace
			if (!ef_ptr) {
				LOG_FATAL(jobType, s_id, log_fp, "can not open [", config.error_path, "]");
				raise(SIGUSR1);
				return false;
			} else {
				error_file.reset(ef_ptr, fclose);
			}
		}
	}

	// redirect stdout -> file
	if (output_file != nullptr) {
		if (dup2(fileno(output_file.get()), fileno(stdout)) == -1) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::DUP2_FAILED, "dup2 output file failed");
			raise(SIGUSR1);
			return false;
		}
	}
	if (error_file != nullptr) {
		// redirect stderr -> file
		if (dup2(fileno(error_file.get()), fileno(stderr)) == -1) {
			LOG_FATAL(jobType, s_id, log_fp, RunnerError::DUP2_FAILED, "dup2 error file failed");
			raise(SIGUSR1);
			return false;
		}
	}

	// set gid
	gid_t group_list[] = {config.gid};
	if (setgid(config.gid) == -1 || setgroups(sizeof(group_list) / sizeof(gid_t), group_list) == -1) {
		LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETUID_FAILED);
		raise(SIGUSR1);
		return false;
	}

	// set uid
	if (setuid(config.uid) == -1) {
		LOG_FATAL(jobType, s_id, log_fp, RunnerError::SETUID_FAILED);
		raise(SIGUSR1);
		return false;
	}

	// load seccomp
	if (config.load_seccomp_rules() != RunnerError::SUCCESS) {
		LOG_FATAL(jobType, s_id, log_fp, RunnerError::LOAD_SECCOMP_FAILED);
		raise(SIGUSR1);
		return false;
	}

	auto args = config.args.getArgs();
	auto env = config.env.getArgs();
	execve(config.exe_path, args.get(), env.get());
	/*
	 * 以上三行不可以并作
	 * execve(config.exe_path, config.args.getArgs().get(), config.env.getArgs().get());
	 * 会取到野指针
	 */

	LOG_FATAL(jobType, s_id, log_fp, RunnerError::EXECVE_FAILED);
	raise(SIGUSR1);
	return false;
}


bool JudgeJob::push_back_failed_judge_job() noexcept
{
	return JudgeJob::insert_into_failed(redisConn, jobType, s_id);
}


bool JudgeJob::insert_into_failed(const kerbal::redis::RedisContext & conn, int jobType, ojv4::s_id_type s_id) noexcept try
{
	LOG_WARNING(jobType, s_id, log_fp, "push back to judge failed list");

	try {
		Result result;
		result.judge_result = UnitedJudgeResult::SYSTEM_ERROR;
		staticCommitJudgeResultToRedis(jobType, s_id, conn, result, -1);

		/*
		 * update update_queue
		 * 当 judge result 更新到 redis 失败时, 千万不可以将 update 任务添加到 update 队列!
		 * 否则 master 会因取不到评测结果导致 update 失败!
		 */
		static RedisCommand update_queue("rpush update_queue %d,%d");
		update_queue.execute(conn, jobType, s_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Commit judge result to redis failed.", e);
	}

	try {
		static RedisCommand update_queue("rpush judge_failure_list %d:%d");
		update_queue.execute(conn, jobType, s_id);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(jobType, s_id, log_fp, "Insert judge failed job into judge failure list failed.", e);
	}

	return true;
} catch (...) {
	UNKNOWN_EXCEPT_FATAL(jobType, s_id, log_fp, "Insert judge failed job into judge failure list failed.");
	return false;
}
