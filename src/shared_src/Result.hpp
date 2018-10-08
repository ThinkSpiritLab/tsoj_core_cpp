/*
 * Result.hpp
 *
 *  Created on: 2018年6月11日
 *      Author: peter
 */

#ifndef SRC_RESULT_HPP_
#define SRC_RESULT_HPP_

#include "united_resource.hpp"

#include <kerbal/utility/memory_storage.hpp>

#include <iostream>
#include <chrono>

struct SolutionDetails
{
		UnitedJudgeResult judge_result;
		std::chrono::milliseconds cpu_time;
		std::chrono::milliseconds real_time;
		kerbal::utility::Byte memory;
		int similarity_percentage;

		SolutionDetails();

		friend std::ostream& operator<<(std::ostream& out, const SolutionDetails & src);
};

struct Result : public SolutionDetails
{
		RunnerError error;
		int signal;
		int exit_code;

		Result();

		void setErrorCode(RunnerError err);

		friend std::ostream& operator<<(std::ostream& out, const Result & src);
};

#endif /* SRC_RESULT_HPP_ */
