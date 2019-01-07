/*
 * mysql_conn_factory.hpp
 *
 *  Created on: 2018年12月3日
 *      Author: peter
 */

#ifndef SRC_MASTER_MYSQL_CONN_FACTORY_HPP_
#define SRC_MASTER_MYSQL_CONN_FACTORY_HPP_

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif
#include <mysql++/connection.h>

#include "sync_nonsingle_instance_pool.hpp"

inline std::string mysql_database;
inline std::string mysql_hostname;
inline std::string mysql_username;
inline std::string mysql_passwd;

typedef sync_nonsingle_instance_pool<mysqlpp::Connection> mysql_conn_pool_type;
inline mysql_conn_pool_type mysql_conn_pool;

inline void add_mysql_conn()
{
	mysqlpp::Connection * mysql_conn = new mysqlpp::Connection(false);
	try {
		mysql_conn->set_option(new mysqlpp::SetCharsetNameOption("utf8"));
		if (!mysql_conn->connect(mysql_database.c_str(), mysql_hostname.c_str(), mysql_username.c_str(), mysql_passwd.c_str())) {
			throw std::runtime_error("failed connect");
		}
	} catch (...) {
		delete mysql_conn;
		mysql_conn = nullptr;
		throw;
	}
	mysql_conn_pool.add(std::move(mysql_conn));
	mysql_conn = nullptr;
}

inline mysql_conn_pool_type::auto_revert_handle sync_fetch_mysql_conn()
{
	mysql_conn_pool_type::auto_revert_handle mysql_conn_handle = mysql_conn_pool.sync_fetch();
	while (!*mysql_conn_handle) {
		mysql_conn_handle.abandon();
		add_mysql_conn();
		mysql_conn_handle = mysql_conn_pool.sync_fetch();
	}
	return mysql_conn_handle;
}


#endif /* SRC_MASTER_MYSQL_CONN_FACTORY_HPP_ */
