/*
 * Result.cpp
 *
 *  Created on: 2018年7月3日
 *      Author: peter
 */


#include "Result.hpp"

SolutionDetails::SolutionDetails() :
		judge_result(UnitedJudgeResult::ACCEPTED),
		cpu_time(0),
		real_time(0),
		memory(0),
		similarity_percentage(0)
{
}

Result::Result() :
		SolutionDetails(),
		error(RunnerError::SUCCESS),
		signal(0), exit_code(0)
{
}

void Result::setErrorCode(RunnerError err)
{
	this->error = err;
	this->judge_result = UnitedJudgeResult::SYSTEM_ERROR;
}

std::ostream& operator<<(std::ostream& out, const SolutionDetails & src)
{
	return out << "result: " << src.judge_result

			<< " cpu_time: " << src.cpu_time.count() << " ms"

			<< " real_time: " << src.real_time.count() << " ms"

			<< " memory: " << src.memory.count() << " Byte"

			<< " similarity_percentage: " << src.similarity_percentage;
}

std::ostream& operator<<(std::ostream& out, const Result & src)
{
	return out << static_cast<const SolutionDetails&>(src)

			<< " error: " << src.error

			<< " signal: " << src.signal

			<< " exit_code: " << src.exit_code;
}


