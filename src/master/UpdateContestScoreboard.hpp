/*
 * UpdateContestScoreboard.hpp
 *
 *  Created on: 2018年10月16日
 *      Author: peter
 */

#ifndef SRC_MASTER_UPDATECONTESTSCOREBOARD_HPP_
#define SRC_MASTER_UPDATECONTESTSCOREBOARD_HPP_

#include <kerbal/redis/redis_context.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>

int update_contest_scoreboard(int ct_id, kerbal::redis::RedisContext redisConn, std::unique_ptr<mysqlpp::Connection> && mysqlConn);

#endif /* SRC_MASTER_UPDATECONTESTSCOREBOARD_HPP_ */
