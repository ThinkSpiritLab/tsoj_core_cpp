/*
 * Config.cpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#include "Config.hpp"

#include "united_resource.hpp"
#include "logger.hpp"
#include <kerbal/compatibility/chrono_suffix.hpp>

#include <seccomp.h>
#include <fcntl.h>

#include "global_shared_variable.hpp"
#include "JudgeJob.hpp"


const int Config::UNLIMITED = -1;
const kerbal::utility::Byte Config::MEMORY_UNLIMITED { 0 };
const std::chrono::milliseconds Config::TIME_UNLIMITED { 0 };

Config::Config()
{
	static const std::string PATH_EQUAL = "PATH=";
	this->env = {PATH_EQUAL + getenv("PATH")};
	this->uid = judger_uid;
	this->gid = judger_gid;
}

RunningConfig::RunningConfig(const JudgeJob & job) :
		Config()
{
	using namespace kerbal::utility;
	using namespace kerbal::compatibility::chrono_suffix;

	static const char c_cpp_exe_path[] = "./Main";
	static const char java_exe_path[] = "/usr/bin/java";

	switch (job.lang) {
		case Language::C:
		case Language::Cpp:
		case Language::Cpp14: {
			this->seccomp_rule_name = Seccomp_rule::c_cpp;
			this->exe_path = c_cpp_exe_path;
			this->max_memory = job.memoryLimit;
			this->max_stack = job.memoryLimit;
			this->max_cpu_time = job.timeLimit;
			this->args = {c_cpp_exe_path};
			break;
		}
		case Language::Java: {
			this->seccomp_rule_name = Seccomp_rule::none;
			this->exe_path = java_exe_path;
			this->max_memory = MEMORY_UNLIMITED;
			this->max_stack = 256_MB;
			this->max_cpu_time = job.timeLimit * 2;
			static boost::format java_xms("-Xms%dm");
			static boost::format java_xmx("-Xmx%dm");
			this->args = {
				java_exe_path,
				(java_xms % job.memoryLimit.count()).str(),
				(java_xmx % (job.memoryLimit.count() + java_memory_bonus.count())).str(),
				"-Djava.security.manager",
				"-Djava.security.policy=" + java_policy_path,
				"Main"
			};
			break;
		}
	}

	this->max_real_time = this->max_cpu_time + 1_s;
	this->max_process_number = UNLIMITED; // TODO
	this->max_output_size = 32_MB;
	this->input_path = NULL;
	this->output_path = "./user.out";
	this->error_path = "./err.out";
}

CompileConfig::CompileConfig(const JudgeJob & job) :
		Config()
{
	using namespace kerbal::utility;
	using namespace kerbal::compatibility::chrono_suffix;

	// https://icpc.baylor.edu/worldfinals/programming-environment
	// 2018 ACM ICPC Programming Environment:
	// gcc -g -O2 -std=gnu11 -static $* -lm
	// g++ -g -O2 -std=gnu++14 -static $*
	// javac -encoding UTF-8 -sourcepath . -d . $*
	// 20180320 add "-Werror=main" in compile_c to avoid "void main()"

	static const char c_compiler[] = "/usr/bin/gcc";
	static const char cpp_compiler[] = "/usr/bin/g++";
	static const char java_compiler[] = "/usr/bin/javac";

	switch (job.lang) {
		case Language::C: {
			this->exe_path = c_compiler;
			this->args = {c_compiler, "-g", "-O2", "-Werror=main", "-std=gnu11", "-static", "Main.c", "-lm", "-o", "Main"};
			this->max_memory = 1024_MB;
			break;
		}
		case Language::Cpp: {
			this->exe_path = cpp_compiler;
			this->args = {cpp_compiler, "-g", "-O2", "-std=gnu++11", "-static", "Main.cpp", "-o", "Main"};
			this->max_memory = 1024_MB;
			break;
		}
		case Language::Cpp14: {
			this->exe_path = cpp_compiler;
			this->args = {cpp_compiler, "-g", "-O2", "-std=gnu++14", "-static", "Main.cpp", "-o", "Main"};
			this->max_memory = 1024_MB;
			break;
		}
		case Language::Java: {
			static boost::format java_xms("-J-Xms%dm");
			static boost::format java_xmx("-J-Xmx%dm");
			this->exe_path = java_compiler;
			this->args = {java_compiler,(java_xms%64).str() , (java_xmx%512).str(), "-encoding", "UTF-8", "-sourcepath", ".", "-d", ".", "Main.java"};
			this->max_memory = Config::MEMORY_UNLIMITED;
			break;
		}
	}
	this->max_cpu_time = 5_s;
	this->max_real_time = 5_s;
	this->max_stack = 1024_MB;
	this->max_process_number = Config::UNLIMITED; //TODO
	this->max_output_size = 128_MB;
	this->input_path = "/dev/null";
	this->output_path = "./compiler.out";
	this->error_path = "./compiler.out";
	this->seccomp_rule_name = Seccomp_rule::none;
}

bool Config::check_is_valid_config() const
{
	using namespace kerbal::utility;
	using namespace std::chrono;
	using namespace kerbal::compatibility::chrono_suffix;

	if ((max_cpu_time < 1_ms && max_cpu_time != TIME_UNLIMITED) ||

	(max_real_time < 1_ms && max_real_time != TIME_UNLIMITED) ||

	(max_stack < 1_MB) || (max_memory < 1_MB && max_memory != MEMORY_UNLIMITED) ||

	(max_process_number < 1 && max_process_number != UNLIMITED) ||

	(max_output_size < 1_MB && max_output_size != MEMORY_UNLIMITED)) {
		return false;
	}
	return true;
}

RunnerError Config::c_cpp_seccomp_rules() const
{
	// load seccomp rules
	// 若使用越权的系统调用之后，则强行终止程序
	scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
	if (!ctx) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// 允许的系统调用列表
	int syscalls_whitelist[] = {
		SCMP_SYS(read), SCMP_SYS(fstat), SCMP_SYS(mmap), SCMP_SYS(mprotect), SCMP_SYS(munmap), SCMP_SYS(uname), SCMP_SYS(arch_prctl), SCMP_SYS(brk), SCMP_SYS(access), SCMP_SYS(exit_group), SCMP_SYS(
				close), SCMP_SYS(readlink), SCMP_SYS(sysinfo), SCMP_SYS(write), SCMP_SYS(writev), SCMP_SYS(lseek) };
	for (int ele : syscalls_whitelist) {
		if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, ele, 0) != 0) {
			return RunnerError::LOAD_SECCOMP_FAILED;
		}
	}
	// add extra rule for execve
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t )(exe_path))) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// do not allow "w" and "rw"
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// 限制生效
	if (seccomp_load(ctx) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	seccomp_release(ctx);
	return RunnerError::SUCCESS;
}

RunnerError Config::general_seccomp_rules() const
{
	// load seccomp rules
	scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
	if (!ctx) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// 禁止的系统调用列表
	int syscalls_blacklist[] = { SCMP_SYS(socket), SCMP_SYS(clone), SCMP_SYS(fork), SCMP_SYS(vfork), SCMP_SYS(kill) };
	for (int ele : syscalls_blacklist) {
		if (seccomp_rule_add(ctx, SCMP_ACT_KILL, ele, 0) != 0) {
			return RunnerError::LOAD_SECCOMP_FAILED;
		}
	}
	// add extra rule for execve
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_NE, (scmp_datum_t )(exe_path))) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// do not allow "w" and "rw"
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// 限制生效
	if (seccomp_load(ctx) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	seccomp_release(ctx);
	return RunnerError::SUCCESS;
}

RunnerError Config::load_seccomp_rules() const
{
	switch (seccomp_rule_name) {
		case Seccomp_rule::none:
			return RunnerError::SUCCESS;
		case Seccomp_rule::c_cpp:
			return c_cpp_seccomp_rules();
		case Seccomp_rule::general:
			return general_seccomp_rules();
		default:
			// rule does not exist
			return RunnerError::LOAD_SECCOMP_FAILED;
	}
}
