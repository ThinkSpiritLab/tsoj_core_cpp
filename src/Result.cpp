/*
 * Result.cpp
 *
 *  Created on: 2018年7月3日
 *      Author: peter
 */


#include "Result.hpp"

Result::Result() :
		result(UnitedJudgeResult::ACCEPTED),
		cpu_time(0), real_time(0), memory(0),
		error(RunnerError::SUCCESS),
		signal(0), exit_code(0)
{
}

void Result::setErrorCode(RunnerError err)
{
	this->error = err;
	this->result = UnitedJudgeResult::SYSTEM_ERROR;
}

std::ostream& operator<<(std::ostream& out, const Result & src)
{
	return out << "result: " << src.result

			<< " cpu_time: " << src.cpu_time.count() << " ms"

			<< " real_time: " << src.real_time.count() << " ms"

			<< " memory: " << src.memory.count() << " Byte"

			<< " error: " << src.error

			<< " signal: " << src.signal

			<< " exit_code: " << src.exit_code;
}
