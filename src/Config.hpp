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
		static constexpr int UNLIMITED = -1;
		static constexpr kerbal::utility::Byte MEMORY_UNLIMITED { 0 };
		static constexpr std::chrono::milliseconds TIME_UNLIMITED { 0 };


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

		bool unlimitCPUTime() const
		{
			return this->max_cpu_time == TIME_UNLIMITED;
		}

		bool unlimitRealTime() const
		{
			return this->max_real_time == TIME_UNLIMITED;
		}

		bool unlimitMemory() const
		{
			return this->max_memory == MEMORY_UNLIMITED;
		}

		bool unlimitStack() const
		{
			return this->max_stack == MEMORY_UNLIMITED;
		}

		bool check_is_valid_config() const
		{
			using namespace std::chrono;
			using namespace kerbal::utility;

			if ((max_cpu_time < milliseconds { 1 } && max_cpu_time != TIME_UNLIMITED) ||

			(max_real_time < milliseconds { 1 } && max_real_time != TIME_UNLIMITED) ||

			(max_stack < 1_MB) || (max_memory < 1_MB && max_memory != MEMORY_UNLIMITED) ||

			(max_process_number < 1 && max_process_number != UNLIMITED) ||

			(max_output_size < 1_MB && max_output_size != MEMORY_UNLIMITED)) {
				return false;
			}
			return true;
		}
};


#endif /* SRC_CONFIG_HPP_ */
