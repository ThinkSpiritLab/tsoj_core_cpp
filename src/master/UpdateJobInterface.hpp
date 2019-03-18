/*
 * UpdateJobInterface.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_UPDATEJOBINTERFACE_HPP_
#define SRC_MASTER_UPDATEJOBINTERFACE_HPP_

#include "JobBase.hpp"
#include "Result.hpp"
#include "db_typedef.hpp"
#include "mysql_empty_res_exception.hpp"
#include "redis_conn_factory.hpp"

#include <boost/current_function.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/connection.h>

/**
 * @interface update 任务接口.
 * @brief JobBase 的子类, 同时是所有类型的 update 类的基类.
 * @note 此类为接口类, 规定了所有 update 类的最小接口. 此类不可实例化.
 */
class UpdateJobInterface: public JobBase
{
	protected:
		oj::u_id_type u_id; ///< @brief user id

		std::string s_posttime; ///< @brief post time

		/**
		 * @brief 构造函数.
		 * @details 获取更新类别任务的额外信息如 u_id, post_time 等.
		 */
		UpdateJobInterface(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	public:
		/**
		 * @brief 执行此 update 任务.
		 * @warning 仅规定了 handle 接口, 具体操作需由子类实现.
		 */
		virtual void handle() override = 0;

	protected:
		/**
		 * @brief 更新用户的提交数、通过数.
		 * @warning 仅规定了更新接口, 具体操作需由子类实现.
		 */
		virtual void update_user(mysqlpp::Connection & mysql_conn) = 0;

		/**
		 * @brief 更新题目的提交数、 通过数.
		 * @warning 仅规定了更新接口, 具体操作需由子类实现.
		 */
		virtual void update_problem(mysqlpp::Connection & mysql_conn) = 0;

		/**
		 * @brief 更新此用户此题的完成状态.
		 * @detail 用户所见到的每题 AC, TO_DO, ATTEMPT 三种状态即为 user problem 表所提供的服务.
		 * @warning 仅规定了更新接口, 具体操作需由子类实现.
		 */
		virtual void update_user_problem(mysqlpp::Connection & mysql_conn) = 0;

	public:

		/**
		 * @brief 析构函数.
		 * @throw 本函数承诺不抛出任何异常.
		 */
		virtual ~UpdateJobInterface() noexcept = default;
};

/**
 * @interface 具体 update 任务接口.
 * @note 此类为接口类, 规定了所有具体 update 类的最小接口. 此类不可实例化.
 */
class ConcreteUpdateJob: virtual protected UpdateJobInterface, protected SolutionDetails
{
	protected:
		oj::s_similarity_type similarity_percentage;

		/**
		 * @brief 构造函数.
		 * @details 获取更新类别任务的额外信息如评测的结果,
		 */
		ConcreteUpdateJob(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

	public:
		/**
		 * @note 将父类的 handle 接口标记为公开.
		 */
		using UpdateJobInterface::handle;

	protected:
		/**
		 * @brief 将本次提交记录更新至 solution 表.
		 * @warning 仅规定了更新接口, 具体操作需由子类实现.
		 */
		virtual void update_solution(mysqlpp::Connection & mysql_conn) = 0;

		/**
		 * @brief 将编译错误信息更新至 MySQL.
		 * @warning 仅规定了更新接口, 具体操作需由子类实现.
		 */
		virtual void update_compile_info(mysqlpp::Connection & mysql_conn) = 0;

		/**
		 * @brief 从 redis 中取得编译错误信息.
		 * @return redis 返回集.
		 * @warning 如果 redis 返回的结果不是字符串类型 (含 nullobj 时), 该方法会通过抛出异常报告错误.
		 * @throws std::runtime_error 如果取得的结果不为字符串类型 (含 nullobj), 则抛出此异常.
		 * @throws std::exception 该方法执行过程中还会因 redis 语句执行失败抛出其他异常.
		 */
		kerbal::redis_v2::reply get_compile_info() const;

		/**
		 * @brief 删除 redis 中本条 job 不再需要的信息.
		 */
 		void clear_this_jobs_info_in_redis() noexcept;

		/**
		 * @brief 如果 update job 的过程中产生失败, 则将记录插入 core_update_failed 表中.
		 * @throws 本方法承诺不抛出任何异常. 因为它本身就是对错误结果的处理函数.
		 * @warning 若此方法在更新表时失败, 则说明 MySQL 连接极可能已失效, 接下来可能会发生一系列未定义行为 (TODO).
		 */
		void core_update_failed_table() noexcept;

	public:

		/**
		 * @brief 析构函数.
		 * @throw 本函数承诺不抛出任何异常.
		 */
		virtual ~ConcreteUpdateJob() noexcept = default;
};

/**
 * @brief ConcreteUpdateJob 工厂函数.
 * @details 根据传入的 jobType, sid 构造一个满足 ConcreteUpdateJob 接口要求的类型的对象, 并以智能指针的形式返回.
 *
 * @return 指向所创建的对象的智能指针.
 * 		- 如果 redis job_info 结构中 is_rejudge 字段为假, 则:
 * 			- 如果 job 为练习, 则实际构造的是一个 BasicExerciseJob 对象
 * 			- 如果 job 为课程, 则实际构造的是一个 BasicCourseJob 对象
 * 			- 如果 job 为竞赛, 则实际构造的是一个 BasicContestJob 对象
 * 		- 如果 is_rejudge 字段为真, 则:
 * 			- 如果 job 为练习, 则实际构造的是一个 RejudgeExerciseJob 对象
 * 			- 如果 job 为课程, 则实际构造的是一个 RejudgeCourseJob 对象
 * 			- 如果 job 为竞赛, 则实际构造的是一个 RejudgeContestJob 对象
 * @warning 返回的对象具有多态性, 请谨慎处理!
 */
std::unique_ptr<ConcreteUpdateJob>
make_update_job(int jobType, oj::s_id_type s_id, kerbal::redis_v2::connection & redis_conn);

#endif /* SRC_MASTER_UPDATEJOBINTERFACE_HPP_ */
