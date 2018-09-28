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

/** 本机主机名 */
std::string host_name;

/** 本机 IP */
std::string ip;

/** 本机当前用户名 */
std::string user_name;

/** 本机监听进程号 */
int listening_pid;

/** 评测服务器id，定义为 host_name:ip */
std::string judge_server_id;

/** 程序的运行目录，用于存放临时文件等 */
std::string init_dir;

/** 测试数据和答案的存放目录 */
std::string input_dir;

/** 日志文件名 */
std::string log_file_name;

/** 日志文件的文件流 */
std::ofstream log_fp;

/** lock_file,用于保证一次只能有一个 judge_server 守护进程在运行 */
std::string lock_file;

/** Linux 中运行评测进程的 user id。 默认为 1666*/
int judger_uid;

/** Linux 中运行评测进程的 group id。 默认为 1666*/
int judger_gid;

/** redis 端口号 */
int redis_port;

/** redis 主机名 */
std::string redis_hostname;

/** redis 最大连接数 */
int max_redis_conn;

/** java 策略 */
std::string java_policy_path;

/** java 额外运行时间值 */
int java_time_bonus;

/** java 额外运行空间值 */
kerbal::utility::MB java_memory_bonus;

/** 已经 AC 代码保存路径，用于查重 */
std::string accepted_solutions_dir;

/** 已经 AC 代码数量 */
int stored_ac_code_max_num;

/**
 * 匿名空间避免其他文件访问。
 * loop 主工作循环
 * cur_running 当前子进程数量
 * max_running 最大子进程数量
 * context_pool redis 连接池
 */
namespace
{
	bool loop = true;
	int cur_running = 0;
	int max_running;
	std::unique_ptr<RedisContextPool> context_pool;
}

/**
 * @brief 发送心跳进程
 * 每隔一段时间，将本机信息提交到数据库中表示当前在线的评测机集合中，表明自身正常工作，可以处理评测任务。
 * @throw 该函数保证不抛出任何异常
 */
void register_self() noexcept try
{
	using namespace std::chrono;
	using namespace kerbal::compatibility::chrono_suffix;

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

/**
 * @brief SIGTERM 信号的处理函数
 * 当收到 SIGTERM 信号，在代评测队列末端加上 type = 0, id = -1 的任务，用于标识结束 slave 工作的意愿。之后在 loop
 * 循环中会据此判断是否结束 slave 进程。
 * @param signum 信号编号
 * @throw 该函数保证不抛出任何异常
 */
void regist_SIGTERM_handler(int signum) noexcept
{
	using namespace kerbal::compatibility::chrono_suffix;
	if (signum == SIGTERM) {
		RedisContext main_conn(redis_hostname.c_str(), redis_port, 1500_ms);
		List<std::string> job_list(main_conn, "judge_queue");
		job_list.push_back(JudgeJob::getExitJobItem());
		LOG_WARNING(0, 0, log_fp,
					"Judge server has received the SIGTERM signal and will exit soon after the jobs are all finished!"_cptr);
	}
}
/**
 * @brief 加载 slave 工作的配置
 * 根据 judge_server.conf 文档，读取工作配置信息。loadConfig 的工作原理详见其文档。
 */
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
		return kerbal::utility::MB{stoull(src)};
	});

	loadConfig.add_rules(accepted_solutions_dir, "accepted_solutions_dir", stringAssign);
	loadConfig.add_rules(stored_ac_code_max_num, "stored_ac_code_max_num", castToInt);

	string buf;
	while (getline(fp, buf)) {
		string key, value;
		std::tie(key, value) = parse_buf(buf);
		if (key != "" && value != "") {
			if (loadConfig.parse(key, value) == false) {
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

/**
 * @brief slave端主程序循环
 * 加载配置信息；连接数据库；取待评测任务信息，交由子进程并评测；创建并分离发送心跳线程
 * @throw UNKNOWN_EXCEPTION
 */
int main(int argc, const char * argv[]) try
{
	using namespace kerbal::compatibility::chrono_suffix;

	if (argc > 1 && argv[1] == std::string("--version")) {
		cout << "version: " __DATE__ " " __TIME__ << endl;
		exit(0);
	}
	cout << std::boolalpha;
	load_config();

	try {
		// +2 : 除了四个评测进程需要 redis 连接以外, 监听队列, 发送心跳进程也各需要一个
		// 当前版本中连接池并未启用
		context_pool.reset(new RedisContextPool(max_running + 2, redis_hostname.c_str(), redis_port, 1500_ms));
	} catch (const std::exception & e) {
		LOG_FATAL(0, 0, log_fp, "Redis connect failed! Error string: "_cptr, e.what());
		exit(-2);
	}

	auto main_conn = context_pool->apply(getpid());
	LOG_INFO(0, 0, log_fp, "main_conn: "_cptr, main_conn.get_id());

	try {
		/**
		 * 创建并分离发送心跳线程
		 */
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
		/*
		 * 当收到 SIGTERM 信号时，会在评测队列末端加一个特殊的评测任务用于标识停止测评。此处若检测到停止
		 * 工作的信号，则结束工作loop
		 * 若取得的是正常的评测任务则继续工作
		 */
		try {
			job_item = job_list.block_pop_front(0_s);
			if (JudgeJob::isExitJob(job_item) == true) {
				loop = false;
				LOG_INFO(0, 0, log_fp, "Get exit job.");
				continue;
			}
			std::tie(job_type, job_id) = JudgeJob::parseJobItem(job_item);
			LOG_DEBUG(job_type, job_id, log_fp, "Judge server "_cptr, judge_server_id, " get job: "_cptr, job_item);
		} catch (const RedisNilException & e) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, e.what(), ", job_item: "_cptr,
					  job_item);
			continue;
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, e.what(), ", job_item: "_cptr,
					  job_item);
			continue;
		} catch (...) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: "_cptr, UNKNOWN_EXCEPTION_WHAT,
					  ", job_item: "_cptr, job_item);
			continue;
		}

		/*
		 * 当子进程数达到最大子进程数设定时，等待一个子进程结束后再运行。
		 * 
		 * 补充一个注释作者一开始没有想明白的地方，觉得有必要在此说明。当 loop 循环人为结束之后，有可能存在部分
		 * 评测子进程并没有结束。因此失去了在此使用 waitpid 获取状态信息并释放的机会。但是这并不会造成僵死进程
		 * 的堆积。当父进程结束之后，子进程的父进程会变为 init 进程，即孤儿进程被收养，而孤儿进程结束之后，会自动调用
		 * waitpid 并释放，因此并不会存在僵死进程堆积。
		 */
		pid_t pid;
		while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
			cur_running--;
		}
		if (cur_running >= max_running) {
			pid = waitpid(-1, NULL, 0);
			cur_running--;
		}

		try {
			Process judge_process(false, [](int job_type, int job_id) {
				RedisContext child_conn(redis_hostname.c_str(), redis_port, 1500_ms);
				if (!child_conn) {
					LOG_FATAL(job_type, job_id, log_fp, "Child context connect failed."_cptr);
					//TODO 插入到 failure list 中
					return;
				}

				std::unique_ptr<JudgeJob> job = nullptr;

				try {
					job.reset(new JudgeJob(job_type, job_id, child_conn));
				} catch (const std::exception & e) {
					LOG_FATAL(job_type, job_id, log_fp, "Fail to create job. Error information: "_cptr, e.what());
					return;
				} catch (...) {
					LOG_FATAL(job_type, job_id, log_fp, "Fail to create job. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
					return;
				}

				// Step 2: Judge job.
				try {
					job->handle();
				} catch (const std::exception & e) {
					LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: "_cptr, e.what());
					job->push_back_failed_judge_job();
					return;
				} catch (...) {
					LOG_FATAL(job_type, job_id, log_fp, "Fail to judge job. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
					job->push_back_failed_judge_job();
					return;
				}
			}, job_type, job_id);
			LOG_DEBUG(job_type, job_id, log_fp, "Judge process fork success. Child_pid: "_cptr, judge_process.get_child_id());
			++cur_running;
		} catch (const std::exception & e) {
			LOG_FATAL(job_type, job_id, log_fp, "Judge process fork failed. Error information: "_cptr, e.what());
			JudgeJob::insert_into_failed(main_conn, job_type, job_id);
			continue;
		} catch (...) {
			LOG_FATAL(job_type, job_id, log_fp, "Judge process fork failed. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
			JudgeJob::insert_into_failed(main_conn, job_type, job_id);
			continue;
		}
	} // loop
	LOG_INFO(0, 0, log_fp, "Judge server exit."_cptr);
	return 0;

} catch (const std::exception & e) {
	LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: "_cptr, e.what());
	throw;
} catch (...) {
	LOG_FATAL(0, 0, log_fp, "An uncatched exception catched by main. Error information: "_cptr, UNKNOWN_EXCEPTION_WHAT);
	throw;
}



