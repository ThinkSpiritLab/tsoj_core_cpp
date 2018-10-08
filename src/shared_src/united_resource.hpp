/*
 * united_resource.hpp
 *
 *  Created on: 2018年6月11日
 *      Author: peter
 */

#ifndef SRC_UNITED_RESOURCE_HPP_
#define SRC_UNITED_RESOURCE_HPP_

#include <iostream>
#include <string>

/**
 * @brief 枚举类，标识测评状态
 */
enum class JudgeStatus
{
	QUEUING = 0, COMPILING = 1, RUNNING = 2, JUDGING = 3, FINISHED = 4, UPDATED = 5
};

/**
 * @brief 枚举类，标识用户所用的语言
 */
enum class Language
{
	C = 0, Cpp = 1, Java = 2, Cpp14 = 3
};

/**
 * @brief 枚举类，标识测评结果
 */
enum class UnitedJudgeResult
{
	/// 编译超时或超内存
	COMPILE_RESOURCE_LIMIT_EXCEED = -2,

	/// 编译错误
	COMPILE_ERROR = -1,

	/// 提交通过
	ACCEPTED = 0,

	/// CPU 时间超时
	CPU_TIME_LIMIT_EXCEEDED = 1,

	/// 墙上时间超时
	REAL_TIME_LIMIT_EXCEEDED = 2,

	/// 超内存
	MEMORY_LIMIT_EXCEEDED = 3,

	/// 运行时错误
	RUNTIME_ERROR = 4,

	/// 系统错误
	SYSTEM_ERROR = 5,

	/// 输出文件大小超过允许值
	OUTPUT_LIMIT_EXCEEDED = 6,

	/// 答案错误
	WRONG_ANSWER = 7,

	/// 输出格式错误
	PRESENTATION_ERROR = 8,

	/// 相似度过高的重复代码
	SIMILAR_CODE = 9
};

/**
 * @brief 根据传入的 UnitedJudgeResult，返回其对应含义的 std::string 类型说明。
 * @param res UnitedJudgeResult 类型
 * @return res 对应含义的 std::string 类型说明。
 */	
std::string getJudgeResultName(UnitedJudgeResult res);

/**
 * @brief 重载 UnitedJudgeResult 类的 << 运算符，输出其对应含义的 std::string
 */
std::ostream& operator<<(std::ostream& out, UnitedJudgeResult res);

/**
 * @brief 枚举类，标识 RE 的类型
 */
enum class RunnerError
{
	SUCCESS = 0,
	INVALID_CONFIG = -1,
	FORK_FAILED = -2,
	PTHREAD_FAILED = -3,
	WAIT_FAILED = -4,
	ROOT_REQUIRED = -5,
	LOAD_SECCOMP_FAILED = -6,
	SETRLIMIT_FAILED = -7,
	DUP2_FAILED = -8,
	SETUID_FAILED = -9,
	EXECVE_FAILED = -10,
	ALREAD_RUNNING = -11,
	CHDIR_ERROR = -12,
};

/**
 * @brief 根据传入的 RunnerError，返回其对应含义的 std::string 类型说明。
 * @param err RunnerError 类型
 * @return err 对应含义的 std::string 类型说明。
 */	
std::string getRunnerErrorName(RunnerError err);

/**
 * @brief 重载 RunnerError 类的 << 运算符，输出其对应含义的 std::string
 */
std::ostream& operator<<(std::ostream& out, RunnerError err);

#endif /* SRC_UNITED_RESOURCE_HPP_ */
