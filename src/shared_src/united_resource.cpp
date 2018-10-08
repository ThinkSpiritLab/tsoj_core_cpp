/*
 * united_resource.cpp
 *
 *  Created on: 2018年7月3日
 *      Author: peter
 */

#include "united_resource.hpp"

std::string getJudgeResultName(UnitedJudgeResult res)
{
	switch (res) {
		case UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED:
			return "COMPILE_RESOURCE_LIMIT_EXCCEED";
		case UnitedJudgeResult::COMPILE_ERROR:
			return "COMPILE_ERROR";
		case UnitedJudgeResult::ACCEPTED:
			return "ACCEPTED";
		case UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED:
			return "CPU_TIME_LIMIT_EXCEEDED";
		case UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED:
			return "REAL_TIME_LIMIT_EXCEEDED";
		case UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED:
			return "MEMORY_LIMIT_EXCEEDED";
		case UnitedJudgeResult::RUNTIME_ERROR:
			return "RUNTIME_ERROR";
		case UnitedJudgeResult::SYSTEM_ERROR:
			return "SYSTEM_ERROR";
		case UnitedJudgeResult::OUTPUT_LIMIT_EXCEEDED:
			return "OUTPUT_LIMIT_EXCEEDED";
		case UnitedJudgeResult::WRONG_ANSWER:
			return "WRONG_ANSWER";
		case UnitedJudgeResult::PRESENTATION_ERROR:
			return "PRESENTATION_ERROR";
		case UnitedJudgeResult::SIMILAR_CODE:
			return "SIMILAR_CODE";
		default:
			return "UNKNOWN RESULT";
	}
}

std::ostream& operator<<(std::ostream& out, UnitedJudgeResult res)
{
	return out << getJudgeResultName(res);
}


std::string getRunnerErrorName(RunnerError err)
{
	switch (err) {
		case RunnerError::SUCCESS:
			return "SUCCESS";
		case RunnerError::INVALID_CONFIG:
			return "INVALID_CONFIG";
		case RunnerError::FORK_FAILED:
			return "FORK_FAILED";
		case RunnerError::PTHREAD_FAILED:
			return "PTHREAD_FAILED";
		case RunnerError::WAIT_FAILED:
			return "WAIT_FAILED";
		case RunnerError::ROOT_REQUIRED:
			return "ROOT_REQUIRED";
		case RunnerError::LOAD_SECCOMP_FAILED:
			return "LOAD_SECCOMP_FAILED";
		case RunnerError::SETRLIMIT_FAILED:
			return "SETRLIMIT_FAILED";
		case RunnerError::DUP2_FAILED:
			return "DUP2_FAILED";
		case RunnerError::SETUID_FAILED:
			return "SETUID_FAILED";
		case RunnerError::EXECVE_FAILED:
			return "EXECVE_FAILED";
		default:
			return "UNKNOWN ERROR";
	}
}

std::ostream& operator<<(std::ostream& out, RunnerError err)
{
	return out << getRunnerErrorName(err);
}
