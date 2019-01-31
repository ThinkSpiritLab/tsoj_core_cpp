/*
 * global_shared_variable.hpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#ifndef SRC_GLOBAL_SHARED_VARIABLE_HPP_
#define SRC_GLOBAL_SHARED_VARIABLE_HPP_

#include <fstream>
#include <string>

extern std::string host_name;
extern std::string ip;
extern std::string user_name;
extern int listening_pid;
extern std::string judge_server_id;

extern std::string init_dir; ///< 程序的运行目录，用于存放临时文件等
extern std::string input_dir;///< 测试数据和答案的存放目录 */
extern std::ofstream log_fp; ///< 日志文件的文件流
extern std::string lock_file; ///< lock_file, 用于保证一次只能有一个 judge_server 守护进程在运行
extern int judger_uid; ///< Linux 中运行评测进程的 user id. 默认为 1666
extern int judger_gid; ///< Linux 中运行评测进程的 group id. 默认为 1666
extern std::string java_policy_path; ///< java 运行策略配置文件路径
extern int java_time_bonus; ///< java 额外运行时间值
extern kerbal::utility::MB java_memory_bonus; ///< java 额外运行空间值
extern std::string accepted_solutions_dir; ///< 已经 AC 代码留样路径，用于查重
extern int stored_ac_code_max_num; ///< 每题最大留样数

#endif /* SRC_GLOBAL_SHARED_VARIABLE_HPP_ */
