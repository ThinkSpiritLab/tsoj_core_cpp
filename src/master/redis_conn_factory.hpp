/*
 * REDIS_conn_factory.hpp
 *
 *  Created on: 2019年1月7日
 *      Author: peter
 */

#ifndef SRC_MASTER_REDIS_CONN_FACTORY_HPP_
#define SRC_MASTER_REDIS_CONN_FACTORY_HPP_

#include <kerbal/redis_v2/connection.hpp>
#include "sync_nonsingle_instance_pool.hpp"

inline int redis_port; ///< redis 端口号
inline std::string redis_hostname; ///< redis 主机名

typedef sync_nonsingle_instance_pool<kerbal::redis_v2::connection> redis_conn_pool_type;
inline redis_conn_pool_type redis_conn_pool;

inline void add_redis_conn()
{
	kerbal::redis_v2::connection * redis_conn = new kerbal::redis_v2::connection(redis_hostname, redis_port);
	if (!*redis_conn) {
	    delete redis_conn;
	    redis_conn = nullptr;
	    throw std::runtime_error("failed connect");
	}
	redis_conn_pool.add(std::move(redis_conn));
	redis_conn = nullptr;
}


inline redis_conn_pool_type::auto_revert_handle sync_fetch_redis_conn()
{
	redis_conn_pool_type::auto_revert_handle redis_conn_handle = redis_conn_pool.sync_fetch();
	while (!*redis_conn_handle) {
		redis_conn_handle.abandon();
		std::cerr << "abandon" << std::endl;
		add_redis_conn();
		redis_conn_handle = redis_conn_pool.sync_fetch();
	}
	return redis_conn_handle;
}



#endif /* SRC_MASTER_REDIS_CONN_FACTORY_HPP_ */
