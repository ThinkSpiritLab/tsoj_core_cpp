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

constexpr int MYSQL_TEXT_MAX_SIZE = 65535;

extern std::string host_name;
extern std::string ip;
extern std::string user_name;
extern int listening_pid;
extern std::string judge_server_id;


//运行目录，存放临时文件等
extern std::string init_dir;

//测试数据和答案的存放目录 这里必须根据测试文件的目录进行修改
extern std::string input_dir;

//日志文件
extern std::ofstream log_fp;

//lock_file,用于保证一次只能有一个judge_server守护进程在运行
extern std::string lock_file;

//Linux中运行评测进程的user。默认为ts_judger,uid=gid=1666
extern int judger_uid;
extern int judger_gid;

//java
extern std::string java_policy_path;
extern int java_time_bonus;
extern int java_memory_bonus;

//已经AC代码保存路径，用于查重
extern std::string accepted_solutions_dir;
extern int stored_ac_code_max_num;

#endif /* SRC_GLOBAL_SHARED_VARIABLE_HPP_ */
