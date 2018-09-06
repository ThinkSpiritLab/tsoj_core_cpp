/*
 * make_update_job.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_MAKE_UPDATE_JOB_HPP_
#define SRC_MASTER_MAKE_UPDATE_JOB_HPP_

#include "logger.hpp"
#include "ExerciseUpdateJob.hpp"
#include "CourseUpdateJob.hpp"
#include "ContestUpdateJob.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <boost/format.hpp>
#include <mysql++/mysql++.h>

extern std::ofstream log_fp;

/**
 * @brief 根据传入的 jobType, sid 构造一个具体的 UpdateJob 对象, 并返回指向所构造对象的智能指针
 * @return 指向构造对象的智能指针.
 *         如果 job 为练习, 则指针实际指向的是一个 ExerciseUpdateJob 对象
 *         如果 job 为课程, 则指针实际指向的是一个 CourseUpdateJob 对象
 *         如果 job 为竞赛, 则指针实际指向的是一个 ContestUpdateJob 对象
 * @warning 返回的对象具有多态性, 请谨慎处理!
 */
inline
std::unique_ptr<UpdateJobBase>
make_update_job(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn)
{
	UpdateJobBase * job = nullptr;

	try {
		if (jobType == 0) {
			int cid = 0;
			static boost::format key_name_tmpl("job_info:%d:%d");
			kerbal::redis::Operation opt(redisConn);

			try {
				cid = opt.hget<int>((key_name_tmpl % jobType % sid).str(), "cid");
			} catch (const std::exception & e) {
				LOG_FATAL(jobType, sid, log_fp, "Get cid failed! Error information: ", e.what());
				throw;
			} catch (...) {
				LOG_FATAL(jobType, sid, log_fp, "Get cid failed! Error information: ", UNKNOWN_EXCEPTION_WHAT);
				throw;
			}
			if (cid != 0) {
				job = new CourseUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
			} else {
				job = new ExerciseUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
			}
		} else {
			job = new ContestUpdateJob(jobType, sid, redisConn, std::move(mysqlConn));
		}
	} catch (const std::exception & e) {
		LOG_FATAL(jobType, sid, log_fp, "Error occurred while construct update job! Error information: ", e.what());
		throw;
	} catch (...) {
		LOG_FATAL(jobType, sid, log_fp, "Error occurred while construct update job! Error information: ", UNKNOWN_EXCEPTION_WHAT);
		throw;
	}
	return std::unique_ptr<UpdateJobBase>(job);
}

#endif /* SRC_MASTER_MAKE_UPDATE_JOB_HPP_ */
