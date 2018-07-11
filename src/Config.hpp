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

class JobInfo;

struct Config
{
		static const int UNLIMITED;
		static const kerbal::utility::Byte MEMORY_UNLIMITED;
		static const std::chrono::milliseconds TIME_UNLIMITED;


		enum TypeFlag
		{
			COMPILE, RUNNING
		};

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
		const char * seccomp_rule_name;
		uid_t uid;
		gid_t gid;

		Config(const JobInfo & job, enum TypeFlag flag);

		void construct_running_config(const JobInfo & job);
		void construct_compile_config(const JobInfo & job);

		bool unlimitCPUTime() const;
		bool unlimitRealTime() const;
		bool unlimitMemory() const;
		bool unlimitStack() const;

		bool check_is_valid_config() const;
};


#endif /* SRC_CONFIG_HPP_ */
