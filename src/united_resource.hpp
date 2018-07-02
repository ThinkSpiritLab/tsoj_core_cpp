/*
 * united_resource.hpp
 *
 *  Created on: 2018年6月11日
 *      Author: peter
 */

#ifndef SRC_UNITED_RESOURCE_HPP_
#define SRC_UNITED_RESOURCE_HPP_

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

inline std::string getJudgeResultName(UnitedJudgeResult res)
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

inline std::ostream& operator<<(std::ostream& out, UnitedJudgeResult res)
{
	return out << getJudgeResultName(res);
}

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

inline std::string getRunnerErrorName(RunnerError err)
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

inline std::ostream& operator<<(std::ostream& out, RunnerError err)
{
	return out << getRunnerErrorName(err);
}

#endif /* SRC_UNITED_RESOURCE_HPP_ */
