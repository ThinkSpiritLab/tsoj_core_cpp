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

enum class JudgeStatus
{
	QUEUING = 0, COMPILING = 1, RUNNING = 2, JUDGING = 3, FINISHED = 4
};

enum class Language
{
	C = 0, Cpp = 1, Java = 2, Cpp14 = 3
};

enum class UnitedJudgeResult
{
	COMPILE_RESOURCE_LIMIT_EXCEED = -2,
	COMPILE_ERROR = -1,
	ACCEPTED = 0,
	CPU_TIME_LIMIT_EXCEEDED = 1,
	REAL_TIME_LIMIT_EXCEEDED = 2,
	MEMORY_LIMIT_EXCEEDED = 3,
	RUNTIME_ERROR = 4,
	SYSTEM_ERROR = 5,
	OUTPUT_LIMIT_EXCEEDED = 6,
	WRONG_ANSWER = 7, // Wrong Answer
	PRESENTATION_ERROR = 8, // PE
	SIMILAR_CODE = 9
};

std::string getJudgeResultName(UnitedJudgeResult res);
std::ostream& operator<<(std::ostream& out, UnitedJudgeResult res);

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
};

std::string getRunnerErrorName(RunnerError err);
std::ostream& operator<<(std::ostream& out, RunnerError err);

#endif /* SRC_UNITED_RESOURCE_HPP_ */
