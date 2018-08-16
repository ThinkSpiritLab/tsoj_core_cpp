/*
 * Config.hpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#ifndef SRC_CONFIG_HPP_
#define SRC_CONFIG_HPP_

#include <kerbal/utility/memory_storage.hpp>
#include <chrono>

#include "ExecuteArgs.hpp"
#include "united_resource.hpp"

class JudgeJob;

enum class Seccomp_rule
{
		none,
		c_cpp,
		general
};

class Config
{
	protected:
		static const int UNLIMITED;
		static const kerbal::utility::Byte MEMORY_UNLIMITED;
		static const std::chrono::milliseconds TIME_UNLIMITED;

	public:

		std::chrono::milliseconds max_cpu_time;
		std::chrono::milliseconds max_real_time;
		kerbal::utility::Byte max_memory;
		kerbal::utility::Byte max_stack;
		int max_process_number;
		kerbal::utility::Byte max_output_size;
		const char * exe_path;
		const char * input_path;
		const char * output_path;
		const char * error_path;
		ExecuteArgs args;
		ExecuteArgs env;
		enum Seccomp_rule seccomp_rule_name;
		uid_t uid;
		gid_t gid;

		Config();

		bool limitCPUTime() const
		{
			return this->max_cpu_time != TIME_UNLIMITED;
		}

		bool limitRealTime() const
		{
			return this->max_real_time != TIME_UNLIMITED;
		}

		bool limitMemory() const
		{
			return this->max_memory != MEMORY_UNLIMITED;
		}

		bool limitStack() const
		{
			return this->max_stack != MEMORY_UNLIMITED;
		}

		bool limitProcessNumber() const
		{
			return this->max_process_number != Config::UNLIMITED;
		}

		bool limitOutput() const
		{
			return this->max_output_size != MEMORY_UNLIMITED;
		}

		bool check_is_valid_config() const;

		RunnerError load_seccomp_rules() const;

	protected:
		RunnerError c_cpp_seccomp_rules() const;
		RunnerError general_seccomp_rules() const;
};

class RunningConfig : public Config
{
	public:
		RunningConfig(const JudgeJob & job);
};

class CompileConfig : public Config
{
	public:
		CompileConfig(const JudgeJob & job);
};


#endif /* SRC_CONFIG_HPP_ */
