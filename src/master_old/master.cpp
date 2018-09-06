#include "logger.hpp"
#include "load_helper.hpp"
#include <iostream>
#include "logger.hpp"
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>
#include "../master_old/UpdatorJob.hpp"
using namespace kerbal::compatibility::chrono_suffix;
using namespace std;

std::ofstream log_fp;

constexpr std::chrono::minutes EXPIRE_TIME { 2 };
constexpr int REDIS_SOLUTION_MAX_NUM = 600;

namespace
{
	bool loop;
	std::string updater_init_dir;
	std::string log_file_name;
	std::string updater_lock_file;
	std::string mysql_hostname;
	std::string mysql_username;
	std::string mysql_passwd;
	std::string mysql_database;
	std::string updater_lock_file;
	int redis_port;
	std::string redis_hostname;
	int max_redis_conn = 1;
}

void load_config()
{
	using namespace kerbal::utility::costream;
	const auto & ccerr = costream<cerr>(LIGHT_RED);

	std::ifstream fp("/etc/ts_judger/updater.conf", std::ios::in); //BUG "re"
	if (!fp) {
		ccerr << "can't not open updater.conf" << endl;
		fp.close();
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
			if(loadConfig.parser(key, value) == false){
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
}


int main()
{
	uid_t uid = getuid();
	if (uid != 0) {
		printf("root required!\n");
		return -1;
	}
	load_config();

	LOG_INFO(0, 0, log_fp, "updater starting ...");

	mysqlpp::Connection mysqlConn("ojv4", "127.0.0.1", "root" , "123");
	RedisContext redis_conn("127.0.0.1", 6309, 100_ms);

#ifdef DEBUG
	if (chdir(updater_init_dir.c_str())) {
		LOG_FATAL(0, 0, log_fp, "Error: ", RunnerError::CHDIR_ERROR);
		return -1;
	}
#else
	if (daemon_init() == FORK_FAILED) {
		ERROR_EXIT_UPDATER (FORK_FAILED);
	}
#endif //DEBUG

	kerbal::redis::List<std::string> update_queue(redis_conn, "update_queue");

	while (loop) {
		int ret1 = 0, ret2 = 0, ret3 = 0, ret4 = 0, ret5 = 0, ret6 = 0;

		std::string job_item = "-1,-1";
		int type = -1;
		int update_id = -1;
		try {
			job_item = update_queue.block_pop_front(0_s);
			std::tie(type, update_id) = JobBase::parseJobItem(job_item);
			LOG_DEBUG(type, update_id, log_fp, "Updator ", " get update job ", job_item);
		} catch (const std::exception & e) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", e.what(), "job_item: ", job_item);
			continue;
		} catch (...) {
			LOG_FATAL(0, 0, log_fp, "Fail to fetch job. Error info: ", "unknown exception", "job_item: ", job_item);
			continue;
		}

		LOG_DEBUG(type, update_id, log_fp, "Updating job");

		UpdatorJob job(type, update_id);

		try {
			job.fetchDetailsFromRedis();
		} catch (...) {
			ret2 = -1;
		}
		Result result;
		try {
			result = job.get_job_result();
		} catch (...) {
			ret3 = -1;
		}

		RedisCommand get_src_code_templ("hget source_code:%d:%d source");
		RedisReply reply = get_src_code_templ.execute(redis_conn, type, update_id);
		if (ret2 || ret3 || reply.replyType() != RedisReplyType::STRING) {
			LOG_FATAL(type, update_id, log_fp, "invalid infomation for updater!");
			continue;
		}

		//判断进入rejudge还是正常update
		if (job.is_rejudge) {
			if (job.rejudge(result)) {
				LOG_FATAL(type, update_id, log_fp, "REJUDGE: failed!");
				continue;
			}
		} else {
			//if(查询)
			//1这个redisreply操作详解，返回值什么的。是否用exist，还是直接用get，（取不到的返回值）2取到了之后，cheat信息保存。 rpush cheat_list user_id
			//3保存完了之后，在solution_manage.py中的get方法中，怎么取redis里面的队列信息
			if (result.judge_result == UnitedJudgeResult::ACCEPTED) {
				RedisCommand get_cheat_status_templ("get cheat_status:%d");
				RedisReply reply2 = get_cheat_status_templ.execute(redis_conn, job.uid);

				switch (reply2.replyType())  //取到则说明该用户在同一时间内ac两题
				{
					case RedisReplyType::STRING: {
						if (job.pid != std::stoi(reply2->str)) {
							kerbal::redis::List<int> cheat_list(redis_conn, "cheat_list");
							cheat_list.push_back(update_id);
						}
						break;
					}
				}
			}
			job.set_cheat_status(EXPIRE_TIME);
			ret1 = job.update_solution(result);
			ret2 = job.update_source_code(reply->str);
			if (!type) {
				if (job.cid > 0) {
					ret3 = job.update_course_user(result);
				}
				ret4 = job.update_user_and_problem(result);
				ret5 = job.update_user_problem(result.judge_result == UnitedJudgeResult::ACCEPTED ? 0 : 1);
			} else {
				ret3 = job.update_contest_user_problem(result.judge_result == UnitedJudgeResult::ACCEPTED ? 1 : 0);
			}
		}

		//保存编译信息
		if (result.judge_result == UnitedJudgeResult::COMPILE_ERROR) {
			ret6 = job.update_compile_info();
			if (ret6) {
				LOG_WARNING(type, update_id, log_fp, "an error occurred while updating compile info");
			}
		}

		if (ret1 || ret2 || ret3 || ret4 || ret5 || ret6) {
			LOG_FATAL(type, update_id, log_fp, "update mysql failed!");
			job.core_update_failed_table(result);
		}

		job.commitJudgeStatusToRedis(JudgeStatus::UPDATED);

		Operation opt(redis_conn);
		opt.del("source_code:%d:%d"_fmt(type, update_id).str());
		opt.del("compile_info:%d:%d"_fmt(type, update_id).str());

		int del_solution = update_id - REDIS_SOLUTION_MAX_NUM;

		int solution_id;
		if (type) {
			solution_id = opt.get<int>("contest:solution_id:%d"_fmt(type).str());
		} else {
			solution_id = opt.get<int>("solution_id");
		}

		if (update_id <= solution_id - REDIS_SOLUTION_MAX_NUM) {
			del_solution = update_id;
		}

		JudgeStatus status = (JudgeStatus) opt.hget<int>("judge_status:%d:%d"_fmt(type, del_solution).str(), "status");
		if (status == JudgeStatus::UPDATED) {
			opt.del("job_info:%d:%d"_fmt(type, del_solution).str());
			opt.del("judge_status:%d:%d"_fmt(type, del_solution).str());
		}
	}
	return 0;
}
