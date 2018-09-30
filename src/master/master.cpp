/*
 * master.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "logger.hpp"
#include "load_helper.hpp"
#include "UpdateJobBase.hpp"

#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_data_struct/set.hpp>
#include <kerbal/system/system.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>

using namespace kerbal::compatibility::chrono_suffix;
using namespace std;

std::ofstream log_fp;

constexpr std::chrono::minutes EXPIRE_TIME = 2_min;
constexpr int REDIS_SOLUTION_MAX_NUM = 600;

namespace
{
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

	bool loop = true;
	std::string updater_init_dir;
	std::string log_file_name;
	std::string updater_lock_file;
	std::string mysql_hostname;
	std::string mysql_username;
	std::string mysql_passwd;
	std::string mysql_database;
	int redis_port;
	std::string redis_hostname;
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

	kerbal::redis::RedisContext regist_self_conn(redis_hostname.c_str(), redis_port, 1500_ms);
	if (!regist_self_conn) {
		LOG_FATAL(0, 0, log_fp, "Regist_self context connect failed.");
		return;
	}
	LOG_INFO(0, 0, log_fp, "Regist_self service get context.");

	static kerbal::redis::Operation opt(regist_self_conn);
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

			static kerbal::redis::Set<std::string> s(regist_self_conn, "online_judger");
			s.insert(judge_server_id);

			LOG_DEBUG(0, 0, log_fp, "Register self success");
			std::this_thread::sleep_for(10_s);
		} catch (const kerbal::redis::RedisException & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Register self failed.", e);
		}
	}
} catch (...) {
	LOG_FATAL(0, 0, log_fp, "Register self failed. Error information: ", UNKNOWN_EXCEPTION_WHAT);
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
		kerbal::redis::RedisContext main_conn(redis_hostname.c_str(), redis_port, 1500_ms);
		kerbal::redis::List<std::string> job_list(main_conn, "judge_queue");
		job_list.push_back(JobBase::getExitJobItem());
		LOG_WARNING(0, 0, log_fp,
					"Master has received the SIGTERM signal and will exit soon after the jobs are all finished!");
	}
}

void load_config()
{
	using namespace kerbal::utility::costream;
	const auto & ccerr = costream<cerr>(LIGHT_RED);

	std::ifstream fp("/etc/ts_judger/updater.conf", std::ios::in); //BUG "re"
	if (!fp) {
		ccerr << "can't not open updater.conf" << endl;
		exit(0);
	}

	LoadConfig<std::string, int> loadConfig;

	auto castToInt = [](const std::string & src) {
		return std::stoi(src);
	};

	auto stringAssign = [](const std::string & src) {
		return src;
	};

	loadConfig.add_rules(updater_init_dir, "updater_init_dir", stringAssign);
	loadConfig.add_rules(log_file_name, "log_file", stringAssign);
	loadConfig.add_rules(updater_lock_file, "updater_lock_file", stringAssign);
	loadConfig.add_rules(mysql_hostname, "mysql_hostname", stringAssign);
	loadConfig.add_rules(mysql_username, "mysql_username", stringAssign);
	loadConfig.add_rules(mysql_passwd, "mysql_passwd", stringAssign);
	loadConfig.add_rules(mysql_database, "mysql_database", stringAssign);
	loadConfig.add_rules(redis_port, "redis_port", castToInt);
	loadConfig.add_rules(redis_hostname, "redis_hostname", stringAssign);

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

	fp.close();

	if (log_file_name.empty()) {
		ccerr << "empty log file name!" << endl;
		exit(0);
	}

	log_fp.open(log_file_name, std::ios::app);
	if (!log_fp) {
		ccerr << "log file open failed" << endl;
		exit(0);
	}

	host_name = get_host_name();
	ip = get_addr_list(host_name).front();
	user_name = get_user_name();
	listening_pid = getpid();
	judge_server_id = host_name + ":" + ip;

	LOG_INFO(0, 0, log_fp, "judge_server_id: ", judge_server_id);
	LOG_INFO(0, 0, log_fp, "listening pid: ", listening_pid);
}


int main() try
{
	uid_t uid = getuid();
	if (uid != 0) {
		cerr << "root required!" << endl;
		exit(-1);
	}
	LOG_INFO(0, 0, log_fp, "Loading finished...");
	load_config();
	LOG_INFO(0, 0, log_fp, "Configure load finished!");

	LOG_INFO(0, 0, log_fp, "Connecting main MYSQL connection...");
	mysqlpp::Connection mainMysqlConn(false);
	mainMysqlConn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
	if(!mainMysqlConn.connect(mysql_database.c_str(), mysql_hostname.c_str(), mysql_username.c_str(), mysql_passwd.c_str()))
	{
		LOG_FATAL(0, 0, log_fp, "Main MYSQL connection connect failed!");
		exit(-1);
	}
	LOG_INFO(0, 0, log_fp, "Main MYSQL connection connected!");

	LOG_INFO(0, 0, log_fp, "Connecting main REDIS connection...");
	kerbal::redis::RedisContext mainRedisConn(redis_hostname.c_str(), redis_port, 100_ms);
	if (!mainRedisConn) {
		LOG_FATAL(0, 0, log_fp, "Main REDIS connection connect failed!");
		exit(-1);
	}
	LOG_INFO(0, 0, log_fp, "Main REDIS connection connected!");

	#ifdef DEBUG
	if (chdir(updater_init_dir.c_str())) {
		LOG_FATAL(0, 0, log_fp, "Error: ", RunnerError::CHDIR_ERROR);
		exit(-1);
	}
	#endif //DEBUG

	try {
		/**
		 * 创建并分离发送心跳线程
		 */
		std::thread regist(register_self);
		regist.detach();
	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "Register self service process fork failed.", e);
		exit(-3);
	}

	LOG_INFO(0, 0, log_fp, "updater starting ...");

	kerbal::redis::List<std::string> update_queue(mainRedisConn, "update_queue");

	while (loop) {

		std::string job_item = "-1,-1";
		int jobType = -1;
		int sid = -1;
		try {
			job_item = update_queue.block_pop_front(0_s);
			if (JobBase::isExitJob(job_item) == true) {
				loop = false;
				LOG_INFO(0, 0, log_fp, "Get exit job.");
				continue;
			}
			std::tie(jobType, sid) = JobBase::parseJobItem(job_item);
			LOG_DEBUG(jobType, sid, log_fp, "Master get update job: ", job_item);
		} catch (const std::exception & e) {
			EXCEPT_FATAL(0, 0, log_fp, "Fail to fetch job.", e, "job_item: ", job_item);
			continue;
		} catch (...) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", "unknown exception", "job_item: ", job_item);
			continue;
		}

		auto start(std::chrono::steady_clock::now());
		LOG_DEBUG(jobType, sid, log_fp, "Update job start");

		kerbal::redis::RedisContext redisConn(redis_hostname.c_str(), redis_port, 100_ms);
		if (!redisConn) {
			LOG_FATAL(jobType, sid, log_fp, "Redis connection connect failed!");
			continue;
		}

		std::unique_ptr <mysqlpp::Connection> mysqlConn(new mysqlpp::Connection(false));
		mysqlConn->set_option(new mysqlpp::SetCharsetNameOption("utf8"));
		if (!mysqlConn->connect(mysql_database.c_str(), mysql_hostname.c_str(), mysql_username.c_str(), mysql_passwd.c_str())) {
			LOG_FATAL(jobType, sid, log_fp, "Mysql connection connect failed!");
			continue;
		}

		std::unique_ptr <UpdateJobBase> job = nullptr;
		try {
			job = make_update_job(jobType, sid, redisConn, std::move(mysqlConn));
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, sid, log_fp, "Job construct failed!", e);
			continue;
		} catch (...) {
			LOG_FATAL(jobType, sid, log_fp, "Job construct failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
			continue;
		}

		try {
			job->handle();
		} catch (const std::exception & e) {
			EXCEPT_FATAL(jobType, sid, log_fp, "Job handle failed!", e);
			continue;
		} catch (...) {
			LOG_FATAL(jobType, sid, log_fp, "Job handle failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
			continue;
		}

		auto end(std::chrono::steady_clock::now());
		auto time_consume = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		LOG_DEBUG(jobType, sid, log_fp, "Update consume: ", time_consume.count());
	}
	return 0;
} catch (const std::exception & e) {
	EXCEPT_FATAL(0, 0, log_fp, "An uncaught exception caught by main.", e);
	throw;
} catch (...) {
	LOG_FATAL(0, 0, log_fp, "An uncaught exception caught by main. Error information: ", UNKNOWN_EXCEPTION_WHAT);
	throw;
}


