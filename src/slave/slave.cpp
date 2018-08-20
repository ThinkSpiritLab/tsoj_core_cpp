/*
 * main.cpp
 *
 *  Created on: 2018年5月29日
 *      Author: peter
 */

#include "process.hpp"
#include "logger.hpp"
#include "load_helper.hpp"
#include <kerbal/redis/redis_context_pool.hpp>
#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_data_struct/set.hpp>
#include "JudgeJob.hpp"
using namespace kerbal::redis;

#include <kerbal/system/system.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>

#include <iostream>
#include <fstream>
#include <cctype>
#include <chrono>
#include <csignal>

using std::cout;
using std::cerr;
using std::endl;

std::string host_name;
std::string ip;
std::string user_name;
int listening_pid;
std::string judge_server_id;

//运行目录，存放临时文件等
std::string init_dir;

//测试数据和答案的存放目录 这里必须根据测试文件的目录进行修改
std::string input_dir;

//日志文件
std::string log_file_name;
std::ofstream log_fp;

//lock_file,用于保证一次只能有一个judge_server守护进程在运行
std::string lock_file;

//Linux中运行评测进程的user。默认为ts_judger,uid=gid=1666
int judger_uid;
int judger_gid;

//设置redis
int redis_port;
std::string redis_hostname;
int max_redis_conn;

//java
std::string java_policy_path;
int java_time_bonus;
kerbal::utility::MB java_memory_bonus;

//已经AC代码保存路径，用于查重
std::string accepted_solutions_dir;
int stored_ac_code_max_num;

namespace
{
	bool loop = true;
	int cur_running = 0;
	int max_running;
	std::unique_ptr<RedisContextPool> context_pool;
}

void register_self() noexcept
{
	using namespace std::chrono;
	using namespace kerbal::compatibility::chrono_suffix;

	try {
		static auto regist_self_conn = context_pool->apply(getpid());
		LOG_INFO(0, 0, log_fp, "Regist_self service get context: "_cptr, regist_self_conn.get_id());

		static Operation opt(regist_self_conn);
		while (true) {
			try {
				const time_t now = time(NULL);
				const std::string confirm_time = get_ymd_hms_in_local_time_zone(now);
				opt.hmset("judge_server:" + judge_server_id,
						"host_name"_cptr, host_name,
						"ip"_cptr, ip,
						"user_name"_cptr, user_name,
						"listening_pid"_cptr, listening_pid,
						"last_confirm"_cptr, confirm_time);
				opt.set("judge_server_confirm:" + judge_server_id, confirm_time);
				opt.expire("judge_server_confirm:" + judge_server_id, 30_s);

				static Set<std::string> s(regist_self_conn, "online_judger");
				s.insert(judge_server_id);

				LOG_DEBUG(0, 0, log_fp, "Register self success");
				std::this_thread::sleep_for(10_s);
			} catch (const RedisException & e) {
				LOG_FATAL(0, 0, log_fp, "Register self failed. Error information: "_cptr, e.what());
			}
		}
	} catch (...) {
		LOG_FATAL(0, 0, log_fp, "Register self failed. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
		exit(2);
	}
}

void regist_SIGTERM_handler(int signum) noexcept
{
	using namespace kerbal::compatibility::chrono_suffix;
	if (signum == SIGTERM) {
		RedisContext main_conn(redis_hostname.c_str(), redis_port, 1500_ms);
		List<std::string> job_list(main_conn, "judge_queue");
		job_list.push_back(JudgeJob::getExitJobItem());
		LOG_WARNING(0, 0, log_fp, "Judge server has received the SIGTERM signal and will exit soon after the jobs are all finished!"_cptr);
	}
}

void load_config()
{
	using std::string;
	using namespace kerbal::utility::costream;
	const auto & ccerr = costream<cerr>(LIGHT_RED);

	std::ifstream fp("/etc/ts_judger/judge_server.conf", std::ios::in); //BUG "re"
	if (!fp) {
		ccerr << "can't not open judge_server.conf" << endl;
		fp.close();
		exit(0);
	}

	LoadConfig<std::string, int, kerbal::utility::MB> loadConfig;

	auto castToInt = [](const std::string & src) {
		return std::stoi(src);
	};

	auto stringAssign = [](const std::string & src) {
		return src;
	};

	loadConfig.add_rules(max_running, "max_running", castToInt);
	loadConfig.add_rules(judger_uid, "judger_uid", castToInt);
	loadConfig.add_rules(judger_gid, "judger_gid", castToInt);
	loadConfig.add_rules(init_dir, "init_dir", stringAssign);
	loadConfig.add_rules(input_dir, "input_dir", stringAssign);
	loadConfig.add_rules(log_file_name, "log_file", stringAssign);
	loadConfig.add_rules(lock_file, "lock_file", stringAssign);
	loadConfig.add_rules(redis_port, "redis_port", castToInt);
	loadConfig.add_rules(redis_hostname, "redis_hostname", stringAssign);
	loadConfig.add_rules(java_policy_path, "java_policy_path", stringAssign);
	loadConfig.add_rules(java_time_bonus, "java_time_bonus", castToInt);
	loadConfig.add_rules(java_memory_bonus, "java_memory_bonus", [](const std::string & src) {
		return kerbal::utility::MB {stoull(src)};
	});

	loadConfig.add_rules(accepted_solutions_dir, "accepted_solutions_dir", stringAssign);
	loadConfig.add_rules(stored_ac_code_max_num, "stored_ac_code_max_num", castToInt);

	string buf;
	while (getline(fp, buf)) {
		string key, value;
		std::tie(key, value) = parse_buf(buf);
		if (key != "" && value != "") {
			if (loadConfig.parser(key, value) == false) {
				ccerr << "unexpected key name" << endl;
			}
			cout << key << " = " << value << endl;
		}
	}
	max_redis_conn = max_running;
	if (log_file_name.empty()) {
		ccerr << "empty log file name!" << endl;
		exit(0);
	}
	
	log_fp.open(log_file_name, std::ios::app);
	if (!log_fp) {
		ccerr << "log file open failed!" << endl;
		exit(0);
	}

	host_name = get_host_name();
	ip = get_addr_list(host_name).front();
	user_name = get_user_name();
	listening_pid = getpid();
	judge_server_id = host_name + ":" + ip;

	LOG_INFO(0, 0, log_fp, "judge_server_id: "_cptr, judge_server_id);
	LOG_INFO(0, 0, log_fp, "listening pid: "_cptr, listening_pid);
}

int main(int argc, const char * argv[])
{
	using namespace kerbal::compatibility::chrono_suffix;

	if (argc > 1 && argv[1] == std::string("--version")) {
		cout << "version: " __DATE__ " " __TIME__ << endl;
		exit(0);
	}
	try {
		cout << std::boolalpha;
		load_config();

		try {
			// +2 : 除了四个评测进程需要 redis 连接以外, 监听队列, 发送心跳进程也各需要一个
			context_pool.reset(new RedisContextPool(max_running + 2, redis_hostname.c_str(), redis_port, 1500_ms));
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Redis connect failed! Error string: "_cptr, e.what());
			exit(-2);
		}

		auto main_conn = context_pool->apply(getpid());
		LOG_INFO(0, 0, log_fp, "main_conn: "_cptr, main_conn.get_id());

		try {
			std::thread regist(register_self);
			regist.detach();
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Register self service process fork failed."_cptr);
			exit(-3);
		}

		signal(SIGTERM, regist_SIGTERM_handler);

		List<std::string> job_list(main_conn, "judge_queue");

		while (loop) {

			// Step 1: Pop job item and parser job_type and job_id.
			std::string job_item = "0,0";
			int job_type = 0;
			int job_id = 0;
			try {
				job_item = job_list.block_pop_front(0_s);
				if (JudgeJob::isExitJob(job_item) == true) {
					loop = false;
					continue;
				}
				std::tie(job_type, job_id) = JudgeJob::parserJobItem(job_item);
				LOG_DEBUG(job_type, job_id, log_fp, "Judge server "_cptr, judge_server_id, " get job: "_cptr, job_item);
			} catch (const RedisNilException & e) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, e.what(), ", job_item: "_cptr, job_item);
				continue;
			} catch (const std::exception & e) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, e.what(), ", job_item: "_cptr, job_item);
				continue;
			} catch (...) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, UNKNOWN_EXCEPTION_WHAT, ", job_item: "_cptr, job_item);
				continue;
			}

			pid_t pid;
			while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
				cur_running--;
			}
			if (cur_running >= max_running) {
				pid = waitpid(-1, NULL, 0);
				cur_running--;
			}

			try {
				Process judge_process(false, [] (int job_type, int job_id) {
					RedisContext child_conn(redis_hostname.c_str(), redis_port, 1500_ms);
					if(!child_conn) {
						LOG_FATAL(job_type, job_id, log_fp, "Child context connect failed."_cptr);
						return;
					}

					JudgeJob job(job_type, job_id, child_conn);

					// Step 2: Get job detail by job_type and job_id.
						try {
							job.fetchDetailsFromRedis();
						} catch (const std::exception & e) {
							LOG_FATAL(job_type, job_id, log_fp, "Fail to fetch job details. Error information: "_cptr, e.what());
							job.push_back_failed_judge_job();
							return;
						} catch (...) {
							LOG_FATAL(job_type, job_id, log_fp, "Fail to fetch job details. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
							job.push_back_failed_judge_job();
							return;
						}

						// Step 3: Judge job.
						try {
							job.judge_job();
						} catch (const std::exception & e) {
							LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: "_cptr, e.what());
							job.push_back_failed_judge_job();
							return;
						} catch (...) {
							LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
							job.push_back_failed_judge_job();
							return;
						}
					}, job_type, job_id);
				LOG_DEBUG(job_type, job_id, log_fp, "Judge process fork success. Child_pid: "_cptr, judge_process.get_child_id());
				++cur_running;
			} catch (const std::exception & e) {
				LOG_FATAL(job_type, job_id, log_fp, "Judge process fork failed. Error information: "_cptr, e.what());
//				job.push_back_failed_judge_job(main_conn);
				continue;
			} catch (...) {
				LOG_FATAL(job_type, job_id, log_fp, "Judge process fork failed. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
//				job.push_back_failed_judge_job(main_conn);
				continue;
			}
		} // loop

	} catch (const std::exception & e) {
		LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: "_cptr, e.what());
	} catch (...) {
		LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
	}

	LOG_INFO(0, 0, log_fp, "Judge server exit."_cptr);
}

