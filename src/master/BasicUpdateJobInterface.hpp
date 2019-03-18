/*
 * BasicUpdateJobInterface.hpp
 *
 *  Created on: 2019年3月11日
 *      Author: peter
 */

#ifndef SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_
#define SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_

#include "UpdateJobInterface.hpp"

class BasicJobInterface : protected ConcreteUpdateJob
{
	protected:
		BasicJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	public:
		/**
		 * @brief 执行任务
		 */
		virtual void handle() override;

	private:
		/**
		 * @brief 将提交代码更新至 mysql
		 * @warning 仅规定 update source_code 表的接口, 具体操作需由子类实现
		 */
		virtual void update_source_code(mysqlpp::Connection & mysql_conn) = 0;

		/**
		 * @brief 删除 redis 中本条 job 向前推 600 条的 job 信息
		 */
 		void clear_previous_jobs_info_in_redis() noexcept;

	public:
		virtual ~BasicJobInterface() noexcept = default;
};

#endif /* SRC_MASTER_BASICUPDATEJOBINTERFACE_HPP_ */
