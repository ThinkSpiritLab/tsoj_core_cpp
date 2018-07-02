/*
 * judge.hpp
 *
 *  Created on: 2018年6月11日
 *      Author: peter
 */

#ifndef SRC_JUDGER_HPP_
#define SRC_JUDGER_HPP_

#include "united_resource.hpp"

#include <chrono>
#include <kerbal/utility/memory_storage.hpp>
#include <iostream>

struct Result
{
		UnitedJudgeResult result;
		std::chrono::milliseconds cpu_time;
		std::chrono::milliseconds real_time;
		kerbal::utility::Byte memory;
		RunnerError error;
		int signal;
		int exit_code;

		Result() :
				result(UnitedJudgeResult::ACCEPTED),
				cpu_time(0), real_time(0), memory(0),
				error(RunnerError::SUCCESS),
				signal(0), exit_code(0)
		{
		}

		void setErrorCode(RunnerError err)
		{
			this->error = err;
			this->result = UnitedJudgeResult::SYSTEM_ERROR;
		}

		friend std::ostream& operator<<(std::ostream& out, const Result & src)
		{
			return out << "result: " << src.result

					<< " cpu_time: " << src.cpu_time.count() << " ms"

					<< " real_time: " << src.real_time.count() << " ms"

					<< " memory: " << src.memory.count() << " Byte"

					<< " error: " << src.error

					<< " signal: " << src.signal

					<< " exit_code: " << src.exit_code;
		}
};

#endif /* SRC_JUDGER_HPP_ */
