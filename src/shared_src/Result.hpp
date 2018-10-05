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

/**
 * @brief 记录编译/运行结果的结构体，包含评测结果，资源使用，退出信息，相似度等方面信息。
 */
struct Result
{
	/** 评测结果 */
	UnitedJudgeResult judge_result;

	/** CPU 时间用量，单位为 ms */
	std::chrono::milliseconds cpu_time;

	/** 墙上时间用量，单位为 ms */
	std::chrono::milliseconds real_time;

	/** 储存空间所用的字节长度，单位为 byte */
	kerbal::utility::Byte memory;

	/** RE 类型 */
	RunnerError error;

	/** 执行中触发的信号 */
	int signal;

	/** 执行程序退出代码 */
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
