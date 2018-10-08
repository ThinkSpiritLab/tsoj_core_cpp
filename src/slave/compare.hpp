/*
 * compare.hpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#ifndef SRC_COMPARE_HPP_
#define SRC_COMPARE_HPP_


#include "united_resource.hpp"

/**
 * @brief 根据用户答案路径和标准答案路径，提取相应文件并进行比对。比对结果作为判题结果，并返回。
 * @param file1 用户答案路径
 * @param file2 标准答案路径
 * @return 表示比对结果的 UnitedJudgeResult 枚举类。
 */	
UnitedJudgeResult compare(const char * file1, const char * file2);


#endif /* SRC_COMPARE_HPP_ */
