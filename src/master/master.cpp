/*
 * master.cpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#include "logger.hpp"
#include "load_helper.hpp"
#include <iostream>
#include <fstream>
#include "logger.hpp"
#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/compatibility/chrono_suffix.hpp>
#include "UpdateJobBase.hpp"
#include "make_update_job.hpp"

using namespace kerbal::compatibility::chrono_suffix;
using namespace std;

std::ofstream log_fp;

constexpr std::chrono::minutes EXPIRE_TIME = 2_min;
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
}


int main()
{
    uid_t uid = getuid();
    if (uid != 0) {
        printf("root required!\n");
        exit(-1);
    }
    load_config();

    LOG_INFO(0, 0, log_fp, "updater starting ...");


//    mysqlpp::Connection mainMysqlConn(false);
//    mainMysqlConn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
//    if(!mainMysqlConn.connect(mysql_database, mysql_hostname, mysql_username, mysql_passwd))
//    {
//        LOG_FATAL(0, 0, log_fp, "Main mysql connection connect failed!");
//        exit(-1);
//    }

    kerbal::redis::RedisContext mainRedisConn(redis_hostname.c_str(), redis_port, 100_ms);
    if (!mainRedisConn) {
        LOG_FATAL(0, 0, log_fp, "Main redis connection connect failed!");
        exit(-1);
    }

#ifdef DEBUG
    if (chdir(updater_init_dir.c_str())) {
        LOG_FATAL(0, 0, log_fp, "Error: ", RunnerError::CHDIR_ERROR);
        exit(-1);
    }
#endif //DEBUG

    kerbal::redis::List <std::string> update_queue(mainRedisConn, "update_queue");

    while (loop) {

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

        LOG_DEBUG(type, update_id, log_fp, "Update job start");

        kerbal::redis::RedisContext redisConn(redis_hostname.c_str(), redis_port, 100_ms);
        if (!redisConn) {
            LOG_FATAL(0, 0, log_fp, "Redis connection connect failed!");
            continue;
        }

        std::unique_ptr <mysqlpp::Connection> mysqlConn(new mysqlpp::Connection(false));
        mysqlConn->set_option(new mysqlpp::SetCharsetNameOption("utf8"));
        if (!mysqlConn->connect(mysql_database.c_str(), mysql_hostname.c_str(), mysql_username.c_str(), mysql_passwd.c_str())) {
            LOG_FATAL(0, 0, log_fp, "Mysql connection connect failed!");
            continue;
        }

        std::unique_ptr <UpdateJobBase> job = nullptr;
        try {
            job = make_update_job(type, update_id, redisConn, std::move(mysqlConn));
        } catch (...) {
            continue;
        }

        try {
            job->handle();
        } catch (...) {

        }

    }
    return 0;
}
