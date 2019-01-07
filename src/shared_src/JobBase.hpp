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
#include <kerbal/utility/storage.hpp>
#include <string>
#include <chrono>

#include "db_typedef.hpp"
#include "united_resource.hpp"

/**
 * @brief JudgeJob 与 UpdateJob 的基类， 存有 Job的基本信息，并规定了所有 Job 必须遵循的函数接口。
 * @note 本类没有具体意义，仅是所有 Job 的共有特征，因此为抽象类，不可实例化。
 */
class JobBase
{
	public:
		int jobType; ///< Job 类型，如：竞赛、课程等

		ojv4::s_id_type s_id; ///< solution id

		kerbal::redis::RedisContext redisConn; ///< redis 连接

	public:
		ojv4::p_id_type p_id; ///< problem id

		Language lang; ///< 语言

		int cases; ///< 测试用例数量

		ojv4::s_time_in_milliseconds timeLimit; ///< 时间限制

		ojv4::s_mem_in_MB memoryLimit; ///< 空间限制

		ojv4::s_similarity_type similarity_threshold; ///< 重复率限制

		/**
		 * @brief 将待处理 Job 信息分解，提取出 job_type 与 job_id
		 * @param jobItem "job_type,job_id" 格式的字符串
		 * @return <job_type, job_id>
		 * @exception std::invalid_argument
		 */
		static std::pair<int, ojv4::s_id_type> parseJobItem(const std::string & jobItem);

		/**
		 * @brief 判断是否终止服务 loop
		 * @param jobItem "job_type,job_id" 格式的字符串
		 * @return 若是检测到停止工作标识，则返回 true ，否则返回 false
		 */
		static bool isExitJob(const std::string & jobItem)
		{
			return jobItem == "0,-1";
		}

		/**
		 * @brief 获取规定的停止工作标识
		 * @return 返回规定的停止工作标识，规定为："0,-1"
		 */
		static std::string getExitJobItem()
		{
			return "0,-1";
		}

		/**
		 * @brief 初始化 jobType, s_id, redisConn
		 * @param jobType Job 类型，如：竞赛、课程等
		 * @param s_id student id
		 * @param redisConn Redis连接
		 * @exception 该构造函数保证不抛出任何异常
		 */
		JobBase(int jobType, ojv4::s_id_type s_id, const kerbal::redis::RedisContext & redisConn);

		/**
		 * @brief 析构函数使用默认析构函数
		 */
		virtual ~JobBase() noexcept = default;

		/**
		 * @brief 本 Job 的处理函数。在基类中定义为纯虚函数。
		 * @warning 本函数为纯虚函数，仅仅规定了接口。
		 */
		virtual void handle() = 0;

		/**
		 * @brief 从 redis 中取得代码
		 * @return Redis 返回集.
		 * @warning 注意! 返回集的类型只可能为字符串类型, 否则该方法会通过抛出异常报告错误
		 * @throws RedisUnexpectedCaseException 如果取得的结果不为字符串类型 (包括空类型), 则抛出此异常
		 * @throws std::exception 该方法执行过程中还会因 redis 操作失败
		 */
		kerbal::redis::RedisReply get_source_code() const;

		/**
		 * @brief 从 redis 数据库获取本 Job 的代码并存储到工作空间中，用于编译运行。
		 * @warning 本函数为纯虚函数，但是有实现。实际上，其子类的部分相同信息可由此函数取得。这部分有：p_id, lang, cases, timeLimit, memoryLimit
		 * @exception JobHandleException
		 */
		void storeSourceCode(const std::string & parent_path = "./", const std::string & file_name = "Main") const;

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
