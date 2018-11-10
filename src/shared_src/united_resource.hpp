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

/*
 * 分享一个这里出现的处理技巧: 对于枚举中未定义的量不应在 switch 语句的 default 分支处理, 而应在函数末尾处理
 * 如果未来加了新定义的枚举值而忘了加上描述, 这样在编译期编译器便会给出警告
 */
inline const char* getJudgeStatusName(JudgeStatus status)
{
	switch(status) {
		case JudgeStatus::QUEUING :
			return "QUEUING";
		case JudgeStatus::COMPILING :
			return "COMPILING";
		case JudgeStatus::RUNNING :
			return "RUNNING";
		case JudgeStatus::JUDGING :
			return "JUDGING";
		case JudgeStatus::FINISHED :
			return "FINISHED";
		case JudgeStatus::UPDATED :
			return "UPDATED";
	}
	return "Unknown JudgeStatus";
}

/**
 * @brief 枚举类，标识用户所用的语言
 */
enum class Language
{
	C = 0, Cpp = 1, Java = 2, Cpp14 = 3
};

inline const char * source_file_suffix(Language lang)
{
	switch (lang) {
		case Language::Cpp:
		case Language::Cpp14:
			return "cpp";
		case Language::C:
			return "c";
		case Language::Java:
			return "java";
	}
	throw std::invalid_argument("Undefined language enumerate");
}

/**
 * @brief 枚举类，标识测评结果
 */
enum class UnitedJudgeResult
{
	COMPILE_RESOURCE_LIMIT_EXCEED = -2, ///< 编译超时或超内存
	COMPILE_ERROR = -1, ///< 编译错误
	ACCEPTED = 0, ///< 提交通过
	CPU_TIME_LIMIT_EXCEEDED = 1, ///< CPU 时间超时
	REAL_TIME_LIMIT_EXCEEDED = 2, ///< 墙上时间超时
	MEMORY_LIMIT_EXCEEDED = 3, ///< 超内存
	RUNTIME_ERROR = 4, ///< 运行时错误
	SYSTEM_ERROR = 5, ///< 系统错误
	OUTPUT_LIMIT_EXCEEDED = 6, ///< 输出文件大小超过允许值
	WRONG_ANSWER = 7, ///< 答案错误
	PRESENTATION_ERROR = 8, ///< 输出格式错误
	SIMILAR_CODE = 9 ///< 相似度过高的重复代码
};

/**
 * @brief 根据传入的 UnitedJudgeResult，返回其对应含义的类型说明。
 * @param res UnitedJudgeResult 类型
 * @return res 对应含义的类型说明。
 */	
inline const char * getJudgeResultName(UnitedJudgeResult res)
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
	}
	return "UNKNOWN RESULT";
}

/**
 * @brief 重载 UnitedJudgeResult 类的 << 运算符，输出其对应含义的类型说明
 */
inline std::ostream& operator<<(std::ostream& out, UnitedJudgeResult res)
{
	return out << getJudgeResultName(res);
}

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
 * @brief 根据传入的 RunnerError，返回其对应含义的类型说明。
 * @param err RunnerError 类型
 * @return err 对应含义的类型说明。
 */	
inline const char* getRunnerErrorName(RunnerError err)
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
		case RunnerError::ALREAD_RUNNING:
			return "ALREAD_RUNNING";
		case RunnerError::CHDIR_ERROR:
			return "CHDIR_ERROR";
	}
	return "UNKNOWN ERROR";
}

/**
 * @brief 重载 RunnerError 类的 << 运算符，输出其对应含义的类型说明
 */
inline std::ostream& operator<<(std::ostream& out, RunnerError err)
{
	return out << getRunnerErrorName(err);
}

#endif /* SRC_UNITED_RESOURCE_HPP_ */
