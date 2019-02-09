/*
 * ProtectedProcess.cpp
 *
 *  Created on: 2018年12月7日
 *      Author: peter
 */

#include "ProtectedProcess.hpp"

#include "process.hpp"
#include "logger.hpp"

#include <thread>
#include <unistd.h>
#include <sys/resource.h>

#include <kerbal/compatibility/chrono_suffix.hpp>

extern std::ofstream log_fp;

namespace
{
	/**
	 * @brief 一个辅助类, 用于确保将打开的文件关闭
	 */
	struct file_guard
	{
			FILE* & file_p;

			file_guard(FILE* & file_p) noexcept :
					file_p(file_p)
			{
			}

			~file_guard() noexcept
			{
				if (file_p == nullptr) {
					return;
				}
				fclose(file_p);
			}
	};

	void __dup(const ProtectedProcessConfig & config)
	{
		{
			FILE * input_file = fopen(config.input_path.c_str(), "r");
			file_guard input_file_guard(input_file);

			if (input_file == nullptr) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::INPUT_FILE, config.input_path);
			}
			// redirect file -> stdin
			// On success, these system calls return the new descriptor.
			// On error, -1 is returned, and errno is set appropriately.
			if (dup2(fileno(input_file), fileno(stdin)) == -1) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::INPUT_FILE, config.input_path);
			}
		}

		{
			FILE * output_file = fopen(config.output_path.c_str(), "w");
			file_guard output_file_guard(output_file);

			if (output_file == nullptr) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::OUTPUT_FILE, config.output_path);
			}
			if (dup2(fileno(output_file), fileno(stdout)) == -1) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::OUTPUT_FILE, config.output_path);
			}
		}

		{
			FILE * error_file = fopen(config.error_path.c_str(), "w");
			file_guard error_file_guard(error_file);

			if (error_file == nullptr) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::ERROR_FILE, config.error_path);
			}
			if (dup2(fileno(error_file), fileno(stderr)) == -1) {
				throw Dup2FailedExcetion(Dup2FailedExcetion::DupFileType::ERROR_FILE, config.error_path);
			}
		}
	}


	void __set_rlimit(const ProtectedProcessConfig & config)
	{
		using namespace std::chrono;
		using namespace kerbal::compatibility::chrono_suffix;
		using namespace kerbal::utility;

		if (config.max_stack) {
			rlim_t rlim = static_cast<rlim_t>(storage_cast<Byte>(config.max_stack.value()).count());
			struct rlimit max_stack = {
				.rlim_cur = rlim,
				.rlim_max = rlim,
			};
			if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
				throw SetRlimitFailedException();
			}
		}

		// set memory limit
		if (config.max_memory) {
			rlim_t rlim = static_cast<rlim_t>(storage_cast<Byte>(config.max_memory.value()).count() * 2);
			struct rlimit max_memory = {
				.rlim_cur = rlim,
				.rlim_max = rlim,
			};
			if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
				throw SetRlimitFailedException();
			}
		}

		// set cpu time limit (in seconds)
		if (config.max_cpu_time) {
			rlim_t rlim = static_cast<rlim_t>(duration_cast<seconds>(config.max_cpu_time.value() + 1000_ms).count());
			struct rlimit max_cpu_time = {
				.rlim_cur = rlim,
				.rlim_max = rlim,
			};
			if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
				throw SetRlimitFailedException();
			}
		}


		// set max process number limit
		if (config.max_process_number) {
			rlim_t rlim = static_cast<rlim_t>(config.max_process_number.value());
			struct rlimit max_process_number = {
				.rlim_cur = rlim,
				.rlim_max = rlim,
			};
			if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
				throw SetRlimitFailedException();
			}
		}


		// set max output size limit
		if (config.max_output_size) {
			rlim_t rlim = static_cast<rlim_t>(storage_cast<Byte>(config.max_output_size.value()).count());
			struct rlimit max_output_size = {
				.rlim_cur = rlim,
				.rlim_max = rlim,
			};
			if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
				throw SetRlimitFailedException();
			}
		}
	}

} /* namespace */

ProtectedProcessDetails
protected_process(const ExecuteArgs & execute_args, const ProtectedProcessConfig & config, const ExecuteArgs & env)
{
	process child_process;
	// 此处创建了一个运行子进程, 该进程内先加载保护策略, 然后使用 execve 函数用 execute_agrs 替换自身
	try {
		child_process = process([&execute_args, &config, &env]() noexcept {

		std::vector<int> v(4000000);
		std::cout << v[4] << std::endl;

			try {
				try {
					__set_rlimit(config);
				} catch(const SetRlimitFailedException & e) {
					raise(SIGUSR1);
					return;
				}

				try {
					__dup(config);
				} catch(const Dup2FailedExcetion & e) {
					raise(SIGUSR1);
					return;
				}
			} catch(...) {
				raise(SIGUSR1);
				return;
			}
			execve(execute_args.getArgs()[0], execute_args.getArgs().get(), env.getArgs().get());
		});
	} catch (const std::exception & e) {
		throw std::runtime_error("fork failed!");
	}

	using namespace std::chrono;
	using namespace kerbal::compatibility::chrono_suffix;

	// record current time
	auto process_start_time_point = high_resolution_clock::now();

	if (config.max_real_time.has_value()) {
		std::thread timeout_killer_thread;
		try {
			timeout_killer_thread = std::thread([&child_process](milliseconds timeout) {
				std::this_thread::sleep_for(timeout + 1_s);
				child_process.kill(SIGKILL);
			}, config.max_real_time.value());
		} catch (const std::system_error & e) {
			child_process.kill(SIGKILL);
			throw ThreadFailedException();
		}

		try {
			timeout_killer_thread.detach();
		} catch (const std::system_error & e) {
			child_process.kill(SIGKILL);
			throw ThreadFailedException();
		}
	}

	ProtectedProcessResult running_result = ProtectedProcessResult::ACCEPTED;
	std::chrono::milliseconds cpu_time;
	std::chrono::milliseconds real_time;
	kerbal::utility::KB memory;
	int exit_code;
	int status;
	{
		struct rusage resource_usage;

		// wait for child process to terminate
		// on success, returns the process ID of the child whose state has changed;
		// On error, -1 is returned.
		if (child_process.join(&status, WSTOPPED, &resource_usage) == -1) {
			child_process.kill(SIGKILL);
			throw std::runtime_error("wait failed!");
		}

		real_time = duration_cast<milliseconds>(high_resolution_clock::now() - process_start_time_point);

		constexpr auto timevalToChrono = [](const timeval & val) -> std::chrono::milliseconds
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(seconds(val.tv_sec) + microseconds(val.tv_usec));
		};

		cpu_time = timevalToChrono(resource_usage.ru_utime) + timevalToChrono(resource_usage.ru_stime);
		memory = kerbal::utility::KB(resource_usage.ru_maxrss);
		exit_code = WEXITSTATUS(status);
	}


	if (exit_code != 0) {
		running_result = ProtectedProcessResult::RUNTIME_ERROR;
		return ProtectedProcessDetails(running_result, real_time, cpu_time, memory, exit_code);
	}

	// if signaled
	if (WIFSIGNALED(status) != 0) {
		LOG_DEBUG(jobType, s_id, log_fp, "signal: ", WTERMSIG(status));
		switch (WTERMSIG(status)) {
			case SIGSEGV:
				if (config.max_memory.has_value() && memory > config.max_memory.value()) {
					running_result = ProtectedProcessResult::MEMORY_LIMIT_EXCEEDED;
				} else {
					running_result = ProtectedProcessResult::RUNTIME_ERROR;
				}
				break;
			case SIGUSR1:
				running_result = ProtectedProcessResult::SYSTEM_ERROR;
				break;
			default:
				running_result = ProtectedProcessResult::RUNTIME_ERROR;
		}
	} else {
		if (config.max_memory.has_value() && memory > config.max_memory.value()) {
			running_result = ProtectedProcessResult::MEMORY_LIMIT_EXCEEDED;
		}
	}
	if (config.max_real_time.has_value() && real_time > config.max_real_time.value()) {
		running_result = ProtectedProcessResult::REAL_TIME_LIMIT_EXCEEDED;
	}
	if (config.max_cpu_time.has_value() && cpu_time > config.max_cpu_time.value()) {
		running_result = ProtectedProcessResult::CPU_TIME_LIMIT_EXCEEDED;
	}
	return ProtectedProcessDetails(running_result, real_time, cpu_time, memory, exit_code);
}
