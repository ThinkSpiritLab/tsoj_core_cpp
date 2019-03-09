/*
 * REDIS_conn_factory.hpp
 *
 *  Created on: 2019年1月7日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_REDIS_CONN_FACTORY_HPP_
#define SRC_SHARED_SRC_REDIS_CONN_FACTORY_HPP_

#include <kerbal/redis_v2/connection.hpp>
#include "sync_nonsingle_instance_pool.hpp"

typedef sync_nonsingle_instance_pool<kerbal::redis_v2::connection> redis_conn_pool_type;
inline redis_conn_pool_type redis_conn_pool;

inline void add_redis_conn(const std::string & hostname, int port)
{
	kerbal::redis_v2::connection * redis_conn = new kerbal::redis_v2::connection(hostname, port);
	try {
		if (!*redis_conn) {
			throw std::runtime_error("failed connect");
		}
	} catch (...) {
		delete redis_conn;
		redis_conn = nullptr;
		throw;
	}
	redis_conn_pool.add(std::move(redis_conn));
	redis_conn = nullptr;
}


inline redis_conn_pool_type::auto_revert_handle sync_fetch_redis_conn()
{
	redis_conn_pool_type::auto_revert_handle redis_conn_handle = redis_conn_pool.sync_fetch();
	while (!*redis_conn_handle) {
		redis_conn_handle.abandon();
		redis_conn_handle = redis_conn_pool.sync_fetch();
	}
	return redis_conn_handle;
}



#endif /* SRC_SHARED_SRC_REDIS_CONN_FACTORY_HPP_ */
