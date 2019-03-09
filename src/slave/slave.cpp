/*
 * main.cpp
 *
 *  Created on: 2018年5月29日
 *      Author: peter
 */

#include "process.hpp"
#include "logger.hpp"
#include "JudgeJob.hpp"
#include "slave_settings.hpp"

#include <kerbal/redis/redis_context_pool.hpp>
#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_data_struct/set.hpp>

using namespace kerbal::redis;

#include "shared_src/redis_conn_factory.hpp"

#include <kerbal/system/system.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>

#include <iostream>
#include <fstream>
#include <cctype>
#include <chrono>
#include <csignal>
#include <thread>
#include <future>

#include <cmdline.h>

#include <boost/filesystem.hpp>

using std::cout;
using std::cerr;
using std::endl;


std::ofstream log_fp;

/**
 * 匿名空间避免其他文件访问。
 * loop 主工作循环
 * cur_running 当前子进程数量
 * max_running 最大子进程数量
 * context_pool redis 连接池
 */
namespace
{
	std::string host_name; ///< 本机主机名
	std::string ip; ///< 本机 IP
	std::string user_name; ///< 本机当前用户名
	int listening_pid; ///< 本机监听进程号
	std::string judge_server_id; ///< 评测服务器id，定义为 host_name:ip
	bool loop = true;
	std::atomic<int> cur_running = 0;
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

	auto redis_conn_handler = sync_fetch_redis_conn();
	auto & regist_self_conn = *redis_conn_handler;
	LOG_INFO(0, 0, log_fp, "Regist_self thread get context.");
	kerbal::redis_v2::operation opt(regist_self_conn);
	kerbal::redis_v2::hash judge_server(regist_self_conn, "judge_server:" + judge_server_id);
	kerbal::redis_v2::set online_judger(regist_self_conn, "online_judger");

	while (loop) {
		try {
			const time_t now = time(NULL);
			const std::string confirm_time = get_ymd_hms_in_local_time_zone(now);
			judge_server.hmset(
				  "host_name", host_name,
				  "ip", ip,
				  "user_name", user_name,
				  "listening_pid", listening_pid,
				  "last_confirm", confirm_time
			);
			opt.setex("judge_server_confirm:" + judge_server_id, 30_s, confirm_time);
			online_judger.sadd(judge_server_id);
			std::this_thread::sleep_for(10_s);
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Register self failed.", e);
		}
	}
} catch (const std::exception & e) {
	EXCEPT_FATAL(0, 0, log_fp, "Regist_self thread works failed.", e);
	exit(2);
} catch (...) {
	UNKNOWN_EXCEPT_FATAL(0, 0, log_fp, "Regist_self thread works failed.");
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
		loop = false;
		LOG_WARNING(0, 0, log_fp,
					"Judge server has received the SIGTERM signal and will exit soon after the jobs are all finished!");
	}
}

/**
 * @brief 加载 slave 工作的配置
 * 根据 judge_server.conf 文档，读取工作配置信息。loadConfig 的工作原理详见其文档。
 */
void load_config(const boost::filesystem::path & config_file)
{
	using std::string;
	using namespace kerbal::utility::costream;
	const auto & ccerr = costream<cerr>(LIGHT_RED);

	extern Settings __settings;

	__settings.parse(config_file);

	log_fp.open(settings.get().runtime.log_file_path.string(), std::ios::app);
	if (!log_fp) {
		ccerr << "log file open failed!" << endl;
		exit(0);
	}


	LOG_INFO(0, 0, log_fp, "judge_server_id: ", judge_server_id);
	LOG_INFO(0, 0, log_fp, "listening pid: ", listening_pid);
}

void listenning_loop()
{
	auto redis_conn_handler = sync_fetch_redis_conn();
	kerbal::redis_v2::connection & main_conn = *redis_conn_handler;
	kerbal::redis_v2::list judge_queue(main_conn, "judge_queue");

	while (loop) {

		// Step 1: Pop job item and parser job_type and job_id.
		int job_type(0);
		ojv4::s_id_type job_id(0);

		/*
		 * 当收到 SIGTERM 信号时，会在评测队列末端加一个特殊的评测任务用于标识停止测评。此处若检测到停止
		 * 工作的信号，则结束工作loop
		 * 若取得的是正常的评测任务则继续工作
		 */
		try {
			std::string job_item = judge_queue.blpop(0_s);
			try {
				std::tie(job_type, job_id) = JudgeJob::parseJobItem(job_item);
			} catch (const std::exception & e) {
				EXCEPT_FATAL(0, 0, log_fp, "Fail to parse job item.", e, "job_item: ", job_item);
				continue;
			}
			LOG_INFO(job_type, job_id, log_fp, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
			LOG_INFO(job_type, job_id, log_fp, "Judge server: ", judge_server_id, " get judge job: ", job_item);
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Fail to fetch job.", e);
			continue;
		} catch (...) {
			UNKNOWN_EXCEPT_FATAL(0, 0, log_fp, "Fail to fetch job.");
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
			process judge_process([](int job_type, ojv4::s_id_type job_id) {
				RedisContext child_conn(redis_hostname.c_str(), redis_port, 1500_ms);
				if (!child_conn) {
					LOG_FATAL(job_type, job_id, log_fp, "Child context connect failed.");
					//TODO 插入到 failure list 中
					return;
				}

				std::unique_ptr<JudgeJob> job = nullptr;

				try {
					job.reset(new JudgeJob(job_type, job_id, child_conn));
				} catch (const std::exception & e) {
					EXCEPT_FATAL(job_type, job_id, log_fp, "Fail to construct job.", e);
					return;
				} catch (...) {
					UNKNOWN_EXCEPT_FATAL(job_type, job_id, log_fp, "Fail to construct job");
					return;
				}

				// Step 2: Judge job.
				try {
					job->handle();
				} catch (const std::exception & e) {
					EXCEPT_FATAL(job_type, job_id, log_fp, "Fail to judge job.", e);
					job->push_back_failed_judge_job();
					return;
				} catch (...) {
					UNKNOWN_EXCEPT_FATAL(job_type, job_id, log_fp, "Fail to judge job.");
					job->push_back_failed_judge_job();
					return;
				}
				
				LOG_INFO(job_type, job_id, log_fp, "Judge finished.");
				LOG_INFO(job_type, job_id, log_fp, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
				
			}, job_type, job_id);
			judge_process.detach();
			LOG_DEBUG(job_type, job_id, log_fp, "Judge process fork success. Child_pid: ", judge_process.get_child_id());
			++cur_running;
		} catch (const std::exception & e) {
			EXCEPT_FATAL(job_type, job_id, log_fp, "Judge process fork failed.", e);
			JudgeJob::insert_into_failed(main_conn, job_type, job_id);
			continue;
		} catch (...) {
			UNKNOWN_EXCEPT_FATAL(job_type, job_id, log_fp, "Judge process fork failed.");
			JudgeJob::insert_into_failed(main_conn, job_type, job_id);
			continue;
		}
	} // loop
}

/**
 * @brief slave 端主程序循环
 * 加载配置信息；连接 redis 数据库；取待评测任务信息，交由子进程并评测；创建并分离发送心跳线程
 * @throw UNKNOWN_EXCEPTION
 */
int main(int argc, char * argv[]) try
{
	using namespace kerbal::compatibility::chrono_suffix;

	cmdline::parser parser;
	parser.add<std::string>("conf", 'c', "Specify configure description file path.", false, "/etc/ts_judger/slave_conf.json");
	parser.add<std::string>("log", 'l', "Specify log file path.", false, "");
	parser.add("skip_root_check", '\0', "Skip root user check while initialzing.");
	parser.add("version", 'v', "Display the version information.");

	parser.parse_check(argc, argv);

	if (parser.exist("version")) {
		std::cout << "Compiled at: " __DATE__ " " __TIME__ << std::endl;
		return 0;
	}

	using namespace kerbal::utility::costream;
	const auto & ccerr = costream<std::cerr>(LIGHT_RED);
	const auto & cwarn = costream<std::cerr>(YELLOW);

	if (parser.exist("skip_root_check")) {
		cwarn << "root check skipped!" << std::endl;
	} else if (getuid() != 0) {
		ccerr << "root required!" << std::endl;
		exit(-1);
	}

	std::string conf = parser.get<std::string>("conf");
	load_config(conf.c_str()); // 提醒: 此函数运行结束以后才可以使用 log 系列宏, 否则 log_fp 没打开
	LOG_INFO(0, 0, log_fp, "Configuration load finished!");

	try {
		boost::filesystem::create_directories(settings.get().judge.workspace_dir);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Make workspace dir failed.", e);
		exit(0);
	}

	for (int i = 0; i < 6; ++i)
		add_redis_conn(settings.get().redis.hostname, settings.get().redis.port);

	std::thread register_self_thread;
	try {
		// 创建并分离发送心跳线程
		register_self_thread = std::thread(register_self);
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Register self service thread detach failed.", e);
		exit(-1);
	}

	signal(SIGTERM, regist_SIGTERM_handler);

	LOG_INFO(0, 0, log_fp, "Judge server starting...");
	listenning_loop();

	register_self_thread.join();

	LOG_INFO(0, 0, log_fp, "Judge server exit.");
	return 0;

} catch (const std::exception & e) {
	EXCEPT_FATAL(0, 0, log_fp, "An uncaught exception caught by main.", e);
	throw;
} catch (...) {
	UNKNOWN_EXCEPT_FATAL(0, 0, log_fp, "An uncaught exception caught by main.");
	throw;
}



