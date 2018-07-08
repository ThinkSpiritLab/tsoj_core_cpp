/*
 * main.cpp
 *
 *  Created on: 2018年5月29日
 *      Author: peter
 */

#include "process.hpp"
#include "logger.hpp"
#include "JobInfo.hpp"

#include <kerbal/redis/redis_context_pool.hpp>
#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_data_struct/set.hpp>
using namespace kerbal::redis;

#include <kerbal/system/system.hpp>

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
int java_memory_bonus;

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
	try {
		static auto regist_self_conn = context_pool->apply(getpid());
		LOG_INFO(0, 0, log_fp, "Regist_self service get context: ", regist_self_conn.get_id());

		while (true) {
			try {
				static Operation opt;
				if (!opt.exists(regist_self_conn, judge_server_id)) {
					opt.hmset(regist_self_conn, judge_server_id,
							"host_name", host_name,
							"ip", ip,
							"user_name", user_name,
							"listening_pid", listening_pid);
				}
				opt.expire(regist_self_conn, judge_server_id, seconds { 30 });

				static Set<std::string> s(regist_self_conn, "online_judger");
				s.insert(judge_server_id);

				LOG_DEBUG(0, 0, log_fp, "Register self success");
				std::this_thread::sleep_for(seconds { 10 });
			} catch (const RedisException & e) {
				LOG_FATAL(0, 0, log_fp, "Register self failed. Error information: ", e.what());
			}
		}
	} catch (...) {
		LOG_FATAL(0, 0, log_fp, "Register self failed. Error information: ", "unknown exception");
		exit(2);
	}
}


void regist_SIGTERM_handler(int signum) noexcept
{
	if (signum == SIGTERM) {
		loop = false;
		LOG_WARNING(0, 0, log_fp, "Judge server has received the SIGTERM signal and will exit soon after the jobs are all finished!");
	}
}

std::pair<std::string, std::string> parse_buf(const std::string & buf)
{
	using std::string;

	const static std::pair<string, string> empty_args_pair;

	if (buf[0] == ';') {
		return empty_args_pair;
	}

	const string::const_iterator it_p = std::find(buf.begin(), buf.end(), '=');
	if (it_p == buf.end()) {
		return empty_args_pair;
	}

	string::const_iterator it_i = std::find_if_not(buf.begin(), buf.end(), isspace);

	string key, value;
	for (; it_i != it_p && !isspace(*it_i); ++it_i) {
		key.push_back(*it_i);
	}

	it_i = std::find_if_not(std::next(it_p), buf.end(), [](char c) {
		return isspace(c) || c == '\"';
	});

	for (; it_i != buf.end(); ++it_i) {
		if (isspace(*it_i) || *it_i == '\"') {
			break;
		}
		value.push_back(*it_i);
	}

	return std::make_pair(key, value);
}

void load_config()
{
	using std::string;
	using std::stoi;

	std::ifstream fp("/etc/ts_judger/judge_server.conf", std::ios::in); //BUG "re"
	if (!fp) {
		LOG_FATAL(0, 0, log_fp, "can't not open judge_server.conf");
		fp.close();
		exit(0);
	}
	fp.close();

	string buf;
	while (getline(fp, buf)) {
		string key, value;

		std::tie(key, value) = parse_buf(buf);
		if (key != "" && value != "") {
			if (key == "max_running") {
				max_running = stoi(value);
			} else if (key == "judger_uid") {
				judger_uid = stoi(value);
			} else if (key == "judger_gid") {
				judger_gid = stoi(value);
			} else if (key == "init_dir") {
				init_dir = value;
			} else if (key == "input_dir") {
				input_dir = value;
			} else if (key == "log_file") {
				log_file_name = value;
			} else if (key == "lock_file") {
				lock_file = value;
			} else if (key == "redis_port") {
				redis_port = stoi(value);
			} else if (key == "redis_hostname") {
				redis_hostname = value;
			} else if (key == "java_policy_path") {
				java_policy_path = value;
			} else if (key == "java_time_bonus") {
				java_time_bonus = stoi(value);
			} else if (key == "java_memory_bonus") {
				java_memory_bonus = stoi(value);
			} else if (key == "accepted_solutions_dir") {
				accepted_solutions_dir = value;
			} else if (key == "stored_ac_code_max_num") {
				stored_ac_code_max_num = stoi(value);
			}
			cout << key << " = " << value << endl;
		}
	}
	max_redis_conn = max_running;
	log_fp.open(log_file_name, std::ios::app);

	host_name = get_host_name();
	ip = get_addr_list(host_name).front();
	user_name = get_user_name();
	listening_pid = getpid();
	judge_server_id = host_name + ":" + ip;

	LOG_INFO(0, 0, log_fp, "judge_server_id: ", judge_server_id);
	LOG_INFO(0, 0, log_fp, "listening pid: ", listening_pid);
	signal(SIGTERM, regist_SIGTERM_handler);
}

int main(int argc, const char * argv[])
{
	if (argc > 1 && argv[1] == std::string("--version")) {
		cout << "version: " __DATE__ " " __TIME__ << endl;
		exit(0);
	}
	try {
		cout << std::boolalpha;
		load_config();

		try {
			// +2 : 除了四个评测进程需要 redis 连接以外, 监听队列, 发送心跳进程也各需要一个
			context_pool.reset(new RedisContextPool(max_running + 2, redis_hostname.c_str(), redis_port, std::chrono::milliseconds { 1500 }));
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Redis connect failed! Error string: ", e.what());
			exit(-2);
		}

		auto main_conn = context_pool->apply(getpid());
		LOG_INFO(0, 0, log_fp, "main_conn: ", main_conn.get_id());

		std::unique_ptr<Process> regist;
		try {
			regist.reset(new Process(true, register_self));
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Register self service process fork failed.");
			exit(-3);
		}

		List<std::string> job_list(main_conn, "judge_queue");

		while (loop) {

			// Step 1: Pop job item and parser job_type and job_id.
			std::string job_item = "-1:-1";
			int job_type = -1;
			int job_id = -1;
			try {
				static constexpr std::chrono::seconds zero_seconds { 0 };
				job_item = job_list.blpop(zero_seconds);
				std::tie(job_type, job_id) = JobInfo::parser_job_item(job_item);
				if (job_type == 0 && job_id == -1) {
					loop = false;
					continue;
				}
				LOG_DEBUG(0, job_id, log_fp, boost::format("Judge server %s get job %s") % judge_server_id % job_item);
			} catch (const RedisNilException & e) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", e.what());
				continue;
			} catch (const std::exception & e) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", e.what());
				continue;
			} catch (...) {
				LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", "unknown exception");
				continue;
			}

			JobInfo job(-1, -1);

			// Step 2: Get job detail by job_type and job_id.
			try {
				job = JobInfo::fetchFromRedis(main_conn, job_type, job_id);
			} catch (const RedisNilException & e) {
				LOG_FATAL(job_type, job_id, log_fp, "Fail to fetch job details. Error information: ", e.what());
				job.push_back_failed_judge_job(main_conn);
				continue;
			} catch (const std::exception & e) {
				LOG_FATAL(job_type, job_id, log_fp, "Fail to fetch job details. Error information: ", e.what());
				job.push_back_failed_judge_job(main_conn);
				continue;
			} catch (...) {
				LOG_FATAL(job_type, job_id, log_fp, "Fail to fetch job details. Error information: ", "unknow exception");
				job.push_back_failed_judge_job(main_conn);
				continue;
			}

			// Step 3: Judge job.
			pid_t pid;
			while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
				cur_running--;
			}
			if (cur_running >= max_running) {
				pid = waitpid(-1, NULL, 0);
				cur_running--;
			}

			try {
				Process judge_process(false, [&job, job_type, job_id] () {
					Context child_conn;
					child_conn.connectWithTimeout(redis_hostname.c_str(), redis_port, std::chrono::milliseconds { 1500 });
					if(!child_conn) {
						LOG_FATAL(job_type, job_id, log_fp, "Child context connect failed.");
						return;
					}
					try {
						job.judge_job(child_conn);
					} catch (const std::exception & e) {
						LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: ", e.what());
						job.push_back_failed_judge_job(child_conn);
					} catch (...) {
						LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: ", "unknow exception");
						job.push_back_failed_judge_job(child_conn);
					}
				});
				LOG_DEBUG(job_type, job_id, log_fp, "Judge process fork success. Child_pid: ", judge_process.get_child_id());
				++cur_running;
			} catch (const std::exception & e) {
				LOG_FATAL(job_type, job_id, log_fp, "Judge process fork failed. Error information: ", e.what());
				job.push_back_failed_judge_job(main_conn);
				continue;
			}
		} // loop

	} catch (const std::exception & e) {
		LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: ", e.what());
	} catch (...) {
		LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: ", "unknown exception");
	}

	LOG_INFO(0, 0, log_fp, "Judge server exit.");
}

