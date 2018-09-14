/*
 * JobBase.hpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#ifndef SRC_JOBBASE_HPP_
#define SRC_JOBBASE_HPP_

#include <kerbal/redis/operation.hpp>
#include <kerbal/redis/redis_context.hpp>
#include <kerbal/utility/memory_storage.hpp>

#include <string>
#include <chrono>

#include "united_resource.hpp"

/**
 * @brief JudgeJob 与 UpdateJob 的基类， 存有 Job的基本信息，并规定了所有 Job 必须遵循的函数接口。
 * @note 本类没有具体意义，仅是所有 Job 的共有特征，因此为抽象类，不可实例化。
 */
class JobBase
{
	public:
		/** Job 类型，如：竞赛、课程等 */
		int jobType;
		/** student id */
		int sid;
		/** redis 连接 */
		kerbal::redis::RedisContext redisConn;

	public:
		/** problem id */
		int pid;
		/** 语言种类 */
		Language lang;
		/** 测试用例数量 */
		int cases; 
		/** 时间限制 */
		std::chrono::milliseconds timeLimit;
		/** 空间限制 */
		kerbal::utility::MB memoryLimit;

		/**
		 * @brief 将待处理 Job 信息分解，提取出 job_type 与 job_id
		 * @param jobItem "job_type,job_id" 格式的字符串
		 * @return
		 * @exception std::invalid_argument
		 */
		static std::pair<int, int> parseJobItem(const std::string & jobItem);

		/**
		 * @brief 初始化 jobType, sid, redisConn
		 * @param jobType Job 类型，如：竞赛、课程等
		 * @param sid student id
		 * @param redisConn Redis连接
		 * @exception 该构造函数保证不抛出任何异常
		 */
		JobBase(int jobType, int sid, const kerbal::redis::RedisContext & redisConn) noexcept;

		/**
		 * @brief 析构函数使用默认析构函数
		 */
		virtual ~JobBase() = default;

		/**
		 * @brief 本 Job 的处理函数。在基类中定义为纯虚函数。
		 * @warning 本函数为纯虚函数，仅仅规定了接口。
		 */
		virtual void handle() = 0;

		/**
		 * @brief 从 redis 数据库获取本 Job 的详细信息。在基类中定义为纯虚函数。
		 * @warning 本函数为纯虚函数，但是有实现。实际上，其子类的部分相同信息可由此函数取得。
		 * @exception std::exception
		 */
		virtual void fetchDetailsFromRedis() = 0;

		/**
		 * @brief 从 redis 数据库获取本 Job 的代码并存储到工作空间中，用于编译运行。
		 * @warning 本函数为纯虚函数，但是有实现。实际上，其子类的部分相同信息可由此函数取得。这部分有：pid, lang, cases, timeLimit, memoryLimit
		 * @exception JobHandleException
		 */
		void storeSourceCode() const;

		/**
		 * @brief 将当前评测状态提交到 redis 数据库。
		 * @exception std::exception
		 */		
		void commitJudgeStatusToRedis(JudgeStatus value);
};

/**
 * @brief 自定义的 Job 异常，继承于 std::exception，专用于 Job 处理中的异常
 */
class JobHandleException: public std::exception
{
	protected:
		/** 异常原因 */
		std::string reason;

	public:
		/**
		 * @brief Job 异常的构造，填入引发异常的原因。
		 */
		JobHandleException(const std::string & reason) :
				reason(reason)
		{
		}

		/**
		 * @brief 获取 Job 异常的原因
		 * @return 指向引发异常原因的 c 风格字符串
		 * @exception 该构造函数保证不抛出任何异常
		 */
		virtual const char * what() const noexcept
		{
			return reason.c_str();
		}
};


#endif /* SRC_JOBBASE_HPP_ */
