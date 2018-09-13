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

/**
 * @brief 枚举类，用于标识程序运行时允许的系统调用类型
 */
enum class Seccomp_rule
{
		none,
		c_cpp,
		general
};

/**
 * @brief 运行配置类，根据具体运行需求提供所需的子进程运行限制条件与子进程运行环境 \n
 * 例如：运行时间限制，运行空间限制，子进程调用系统函数的权限等
 * @warning 对于时间空间等限制与子进程运行环境，此类仅仅提供了限制条件的具体值，但并未应用，即并未真正对进程做出限制。真正做出限制由 JudgeJob 自行完成。 \n
 * 但子进程调用系统函数的权限限制，本类提供了接口函数直接使其生效。
 */
class Config
{
	protected:
		/** 常量 -1，用于表示无限制 */
		static const int UNLIMITED;
		/** 常量，用于表示无空间资源限制 */
		static const kerbal::utility::Byte MEMORY_UNLIMITED;
		/** 常量，用于表示无时间资源限制 */
		static const std::chrono::milliseconds TIME_UNLIMITED;

	public:
		/** 最大 CPU 时间限制，单位为 ms */
		std::chrono::milliseconds max_cpu_time;
		/** 最大墙上时间，单位为 ms */
		std::chrono::milliseconds max_real_time;
		/** 储存空间的最大字节长度，单位为 byte */
		kerbal::utility::Byte max_memory;
		/** 栈的最大字节长度，单位为 byte */
		kerbal::utility::Byte max_stack;
		/** 程序最大子进程数 */
		int max_process_number;
		/** 可创建的文件的最大字节长度，即为输出字节的最大长度 */
		kerbal::utility::Byte max_output_size;
		/** 新执行程序的路径名 */
		const char * exe_path;
		/** 标准输入流路径 */
		const char * input_path;
		/** 标准输出流路径 */
		const char * output_path;
		/** 标准错误流路径 */
		const char * error_path;
		/** 新执行程序的参数表 */
		ExecuteArgs args;
		/** 新执行程序的环境表 */
		ExecuteArgs env;
		/** 枚举变量，程序运行时允许的系统调用类型 */
		enum Seccomp_rule seccomp_rule_name;
		/** 进程的 user id */
		uid_t uid;
		/** 进程的 group id */
		gid_t gid;

		Config();

		/**
		 * @brief 检查是否有 CPU 时间限制
		 * @return 若有 CPU 时间限制，返回 true，否则返回 false
		 */
		bool limitCPUTime() const
		{
			return this->max_cpu_time != TIME_UNLIMITED;
		}

		/**
		 * @brief 检查是否有墙上时间限制
		 * @return 若有墙上时间限制，返回 true，否则返回 false
		 */
		bool limitRealTime() const
		{
			return this->max_real_time != TIME_UNLIMITED;
		}

		/**
		 * @brief 检查是否有储存空间的最大字节长度限制
		 * @return 若有储存空间的最大字节长度限制，返回 true，否则返回 false
		 */
		bool limitMemory() const
		{
			return this->max_memory != MEMORY_UNLIMITED;
		}

		/**
		 * @brief 检查是否有栈的最大字节长度限制
		 * @return 若有栈的最大字节长度限制，返回 true，否则返回 false
		 */
		bool limitStack() const
		{
			return this->max_stack != MEMORY_UNLIMITED;
		}

		/**
		 * @brief 检查是否有程序最大子进程数限制
		 * @return 若有程序最大子进程数限制，返回 true，否则返回 false
		 */
		bool limitProcessNumber() const
		{
			return this->max_process_number != Config::UNLIMITED;
		}

		/**
		 * @brief 检查是否有最大输出字节长度限制
		 * @return 若有最大输出字节长度限制，返回 true，否则返回 false
		 */
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
