/*
 * Config.cpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#include "Config.hpp"
#include "united_resource.hpp"
#include "JobInfo.hpp"
#include "logger.hpp"
#include "global_shared_variable.hpp"

Config::Config(const JobInfo & job, enum TypeFlag flag)
{
	switch (flag) {
		case RUNNING:
			try {
				construct_running_config(job);
			} catch (const std::exception & e) {
				LOG_FATAL(job.jobType, job.sid, log_fp, "Fail to construct config. Error information: ", e.what());
				throw;
			} catch (...) {
				LOG_FATAL(job.jobType, job.sid, log_fp, "Fail to construct config. Error information: ", "unknow exception");
				throw;
			}
			break;
		case COMPILE:
			try {
				construct_compile_config(job);
			} catch (const std::exception & e) {
				LOG_FATAL(job.jobType, job.sid, log_fp, "Fail to construct config. Error information: ", e.what());
				throw;
			} catch (...) {
				LOG_FATAL(job.jobType, job.sid, log_fp, "Fail to construct config. Error information: ", "unknow exception");
				throw;
			}
			break;
	}
}

void Config::construct_running_config(const JobInfo & job)
{
	using namespace kerbal::utility;

	switch (job.lang) {
		case Language::C:
		case Language::Cpp:
		case Language::Cpp14: {
			this->seccomp_rule_name = "c_cpp";
			this->exe_path = "./Main";
			this->max_memory = job.memoryLimit;
			this->max_stack = job.memoryLimit;
			this->max_cpu_time = job.timeLimit;
			this->args = {"./Main"};
			break;
		}
		case Language::Java: {
			this->seccomp_rule_name = NULL;
			this->exe_path = "/usr/bin/java";
			this->max_memory = MEMORY_UNLIMITED;
			this->max_stack = 256_MB;
			this->max_cpu_time = job.timeLimit * 2;
			static boost::format java_xms("-Xms%dm");
			static boost::format java_xmx("-Xmx%dm");
			this->args = {
				"/usr/bin/java",(java_xms % job.memoryLimit.count()).str(),
				(java_xmx % (job.memoryLimit.count() + java_memory_bonus)).str(),
				"-Djava.security.manager",
				"-Djava.security.policy=" + java_policy_path,
				"Main"};
			break;
		}
		default:
			LOG_FATAL(job.jobType, job.sid, log_fp, "language unknown");
			break;
	}

	this->max_real_time = this->max_cpu_time + std::chrono::seconds { 1 };
	this->max_process_number = UNLIMITED; // TODO
	this->max_output_size = 32_MB;
	this->input_path = NULL;
	this->output_path = "./user.out";
	this->error_path = "./err.out";
	this->env = {std::string("PATH=") + getenv("PATH")};
	this->uid = judger_uid;
	this->gid = judger_gid;
}

void Config::construct_compile_config(const JobInfo & job)
{
	using namespace kerbal::utility;

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
			break;
		}
		case Language::Cpp: {
			this->exe_path = cpp_compiler;
			this->args = {cpp_compiler, "-g", "-O2", "-std=gnu++11", "-static", "Main.cpp", "-o", "Main"};
			break;
		}
		case Language::Cpp14: {
			this->exe_path = cpp_compiler;
			this->args = {cpp_compiler, "-g", "-O2", "-std=gnu++14", "-static", "Main.cpp", "-o", "Main"};
			break;
		}
		case Language::Java: {
			static boost::format java_xms("-J-Xms%dm");
			static boost::format java_xmx("-J-Xmx%dm");
			this->exe_path = java_compiler;
			this->args = {java_compiler,(java_xms%64).str() , (java_xmx%512).str(), "-encoding", "UTF-8", "-sourcepath", ".", "-d", ".", "Main.java"};
			break;
		}
		default: {
			LOG_FATAL(job.jobType, job.sid, log_fp, "language unknown");
			return;
		}
	}
	this->max_cpu_time = std::chrono::milliseconds { 5000 };
	this->max_real_time = std::chrono::milliseconds { 5000 };

	switch (job.lang) {
		case Language::Java:
			this->max_memory = Config::MEMORY_UNLIMITED;
			break;
		default:
			this->max_memory = 1024_MB;
	}
	this->max_stack = 1024_MB;
	this->max_process_number = Config::UNLIMITED; //TODO
	this->max_output_size = 128_MB;
	this->input_path = "/dev/null";
	this->output_path = "./compiler.out";
	this->error_path = "./compiler.out";
	this->env = {std::string("PATH=") + getenv("PATH")};
	this->seccomp_rule_name = NULL;
	this->uid = judger_uid;
	this->gid = judger_gid;
}
