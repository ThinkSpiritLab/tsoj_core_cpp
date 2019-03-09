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


typedef sync_nonsingle_instance_pool<mysqlpp::Connection> mysql_conn_pool_type;
inline mysql_conn_pool_type mysql_conn_pool;

inline void add_mysql_conn()
{
	mysqlpp::Connection * mysql_conn = new mysqlpp::Connection(false);
	try {
		mysql_conn->set_option(new mysqlpp::SetCharsetNameOption("utf8"));
		if (!mysql_conn->connect(
				settings.get().mysql.database.c_str(),
				settings.get().mysql.hostname.c_str(),
				settings.get().mysql.username.c_str(),
				settings.get().mysql.passward.c_str(),
				settings.get().mysql.port
			)) {
			throw std::runtime_error("failed connect");
		}
	} catch (...) {
		delete mysql_conn;
		mysql_conn = nullptr;
		throw;
	}
	mysql_conn_pool.add(mysql_conn);
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
