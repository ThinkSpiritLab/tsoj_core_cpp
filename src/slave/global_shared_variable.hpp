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


/** 程序的运行目录，用于存放临时文件等 */
extern std::string init_dir;

/** 测试数据和答案的存放目录 */
extern std::string input_dir;

/** 日志文件的文件流 */
extern std::ofstream log_fp;

/** lock_file,用于保证一次只能有一个 judge_server 守护进程在运行 */
extern std::string lock_file;

/** Linux 中运行评测进程的 user id。 默认为 1666*/
extern int judger_uid;

/** Linux 中运行评测进程的 group id。 默认为 1666*/
extern int judger_gid;

/** java 策略 */
extern std::string java_policy_path;

/** java 额外运行时间值 */
extern int java_time_bonus;

/** java 额外运行空间值 */
extern kerbal::utility::MB java_memory_bonus;

/** 已经 AC 代码保存路径，用于查重 */
extern std::string accepted_solutions_dir;

/** 已经 AC 代码数量 */
extern int stored_ac_code_max_num;

#endif /* SRC_GLOBAL_SHARED_VARIABLE_HPP_ */
