/*
 * UpdateJobBase.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_UPDATEJOBBASE_HPP_
#define SRC_MASTER_UPDATEJOBBASE_HPP_

#include "JobBase.hpp"
#include "mysql_empty_res_exception.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>

/**
 * @brief 枚举类，标识每道题目对一个用户的状态
 */
enum class user_problem_status
{
	TODO = -1,
	ACCEPTED = 0,
	ATTEMPTED = 1,
};

/**
 * @brief JobBase 的子类，同时是所有类型的 update 类的基类。
 * @note 此类为虚基类，update_solution 等函数需要由子类实现，此类不可实例化
 */
class UpdateJobBase: public JobBase
{
	private:
		typedef JobBase supper_t;

	protected:

		using RedisContext = kerbal::redis::RedisContext;

		std::unique_ptr<mysqlpp::Connection> mysqlConn;

		///@brief user id
		int uid;

		///@brief course_id
		int cid;

		///@brief post time
		std::string postTime;

		///@brief whether the user has pass the problem before
		//bool haveAccepted;

		///@brief whether store the user's code if this solution is accepted
		bool no_store_ac_code;

		///@brief is rejudge job
		bool is_rejudge;

		/**
		 * @brief 需要同步到 mysql 数据库中的结果
		 * @note 此处没有使用 Result.hpp 中定义的 Result，因为其中含有部分不需提交到 mysql 的参数。此处的 Result
		 * 作为内部结构体，也不会轻易暴露在外造成混淆。
		 */
		struct Result
		{
			/** 评测结果 */
			UnitedJudgeResult judge_result;
			/** CPU 时间用量，单位为 ms */
			std::chrono::milliseconds cpu_time;
			/** 墙上时间用量，单位为 ms */
			std::chrono::milliseconds real_time;
			/** 储存空间所用的字节长度，单位为 byte */
			kerbal::utility::Byte memory;
			/** 代码查重的相似度 */
			int similarity_percentage;
		} result;

	protected:
		// make_update_job 为一个全局函数，用于根据提供的信息生成一个具体的 UpdateJobBase 信息，
		// 而 UpdateJobBase的实例化应该被控制，将随意 new 一个出来的可能性暴露出来是不妥当的，
		// 因此构造函数定义为 protected，只能由 make_update_job 来生成，故 make_update_job 也需要成为友元函数。
		friend
		std::unique_ptr<UpdateJobBase>
		make_update_job(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);
		/**
		 * @brief 通用基类的构造函数，除父类 JobBase 构造函数获得的信息除外，还额外获取更新类别任务的额外信息如 uid,
		 * cid, postTime等信息。
		 */
		UpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
						std::unique_ptr<mysqlpp::Connection> && mysqlConn);
	public:
		/**
		 * @brief 执行任务
		 * @warning 本方法实现了基类中规定的 void handle() 接口, 且子类不可再覆盖
		 */
		virtual void handle() override final;

	protected:
		/**
		 * @brief 将本次提交记录更新至 solution 表
		 * @warning 仅规定 update solution 表的接口, 具体操作需由子类实现
		 */
		virtual void update_solution() = 0;

	private:
		/**
		 * @brief 将提交代码更新至 mysql
		 * @param source_code 指向代码字符串的常量指针
		 * @warning 不建议子类重写该方法
		 */
		void update_source_code(const char * source_code);

		/**
		 * @brief 将编译错误信息更新至 mysql
		 * @param source_code 指向编译错误信息字符串的常量指针
		 * @warning 不建议子类重写该方法
		 */
		void update_compile_info(const char * compile_info);

		/**
		 * @brief 从 redis 中取得编译错误信息
		 * @return Redis 返回集. 注意! 返回集的类型只可能为字符串类型, 否则该方法会通过抛出异常报告错误
		 * @throws RedisUnexpectedCaseException 如果取得的结果不为字符串类型 (包括空类型), 则抛出此异常
		 * @throws std::exception 该方法执行过程中还会因 redis 操作失败
		 */
		kerbal::redis::RedisReply get_compile_info() const;

	protected:
		/**
		 * @brief 更新题目的提交数, 通过数, 用户的提交数, 通过数
		 * @warning 仅规定 update user and problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_and_problem() = 0;

		/**
		 * @brief 更新此用户此题的完成状态
		 * @detail 用户所见到的每题 AC, TO_DO, ATTEMPT 三种状态即为user problem 表所提供的服务
		 * @warning 仅规定 update user problem 表的接口, 具体操作需由子类实现
		 */
		virtual void update_user_problem() = 0;

	private:

		/**
		 * @brief 如果 update job 的过程中产生失败, 则将记录插入 core_update_failed 表中
		 * @throws 本方法承诺不抛出任何异常. 因为它本身就是对错误结果的处理函数
		 * @warning 若此方法在更新表时失败, 则说明 mysql 连接可能已失效, 接下来可能会发生一系列未定义行为 (TODO)
		 */
		void core_update_failed_table(const kerbal::redis::RedisReply & source_code, const kerbal::redis::RedisReply & compile_error_info) noexcept;

		/**
		 * @brief 删除 redis 中本条 job 向前推 600 条的 job 信息
		 */
 		void clear_previous_jobs_info_in_redis() noexcept;

		/**
		 * @brief 删除 redis 中本条 job 不再需要的信息
		 */
 		void clear_this_jobs_info_in_redis() noexcept;

	public:
		virtual ~UpdateJobBase() noexcept = default;
};

/**
 * @brief 根据传入的 jobType, sid 构造一个具体的 UpdateJob 对象, 并返回指向所构造对象的智能指针
 * @return 指向构造对象的智能指针.
 *         如果 job 为练习, 则指针实际指向的是一个 ExerciseUpdateJob 对象
 *         如果 job 为课程, 则指针实际指向的是一个 CourseUpdateJob 对象
 *         如果 job 为竞赛, 则指针实际指向的是一个 ContestUpdateJob 对象
 * @warning 返回的对象具有多态性, 请谨慎处理!
 */
std::unique_ptr<UpdateJobBase>
make_update_job(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
				std::unique_ptr<mysqlpp::Connection> && mysqlConn);

#endif /* SRC_MASTER_UPDATEJOBBASE_HPP_ */
