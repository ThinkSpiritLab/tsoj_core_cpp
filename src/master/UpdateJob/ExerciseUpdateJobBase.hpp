/*
 * ExerciseUpdateJobBase.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_
#define SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_

#include "UpdateJobBase.hpp"

/**
 * @brief ExerciseUpdateJob 与 CourseUpdateJob 的虚基类，定义了解题状态查询与更新提交通过数等通用部分。
 */
class ExerciseUpdateJobBase : public UpdateJobBase
{
	protected:
		ExerciseUpdateJobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn);


	private:
		/**
		 * @brief 将提交代码更新至 mysql
		 */
		virtual void update_source_code(mysqlpp::Connection & mysql_conn) override final;

		/**
		 * @brief 将编译错误信息更新至 mysql
		 */
		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) override final;

	protected:

		virtual ~ExerciseUpdateJobBase() noexcept = default;
};

#endif /* SRC_MASTER_EXERCISEUPDATEJOBBASE_HPP_ */
