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

		/** 代码查重的相似度 */
		int similarity_percentage;

		/**
		 * @brief Result 构造函数，结果设定 为AC，资源用量、信号、退出代码、相似度置为 0
		 */
		Result();
	
		/**
		 * @brief 将本结构体的 RunnerError 置为 err， 并将评测结果标为 UnitedJudgeResult::SYSTEM_ERROR
		 * @param err 表示 RunnerError 的类型
		 */
		void setErrorCode(RunnerError err);

		/**
		 * @brief 重载 << 运算符，
		 */
		friend std::ostream& operator<<(std::ostream& out, const Result & src);
};

#endif /* SRC_RESULT_HPP_ */
