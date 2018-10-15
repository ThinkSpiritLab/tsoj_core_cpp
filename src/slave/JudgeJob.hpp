/*
 * JobInfo.hpp
 *
 *  Created on: 2018年6月8日
 *      Author: peter
 */

#ifndef SRC_JOBINFO_HPP_
#define SRC_JOBINFO_HPP_

#include "JobBase.hpp"
#include "Result.hpp"

class Config;

/**
 * @brief JobBase 的子类，用于实现提交代码的判题功能。
 */
class JudgeJob: public JobBase
{
	private:
		typedef JobBase supper_t;

	protected:
		/** 本 job 临时工作路径 */
		const std::string dir;

		int have_accepted;
		int no_store_ac_code;

	public:

		/**
		 * @brief 初始化 job 基本信息，从数据库获得 job 详细内容
		 * @param jobType Job 类型，如：竞赛、课程等
		 * @param sid student id
		 * @param redisConn Redis连接
		 * @exception std::exception 等，表示获取 job 详细信息失败
		 */
		JudgeJob(int jobType, int sid, const kerbal::redis::RedisContext & conn);

		/**
		 * @brief 本 Job 的处理函数。整个 job 的工作入口，包括编译执行判题等内容
		 * @warning 本函数为虚函数，是对父类接口的实现。
		 */
		virtual void handle() override;

	protected:
		/**
		 * @brief 将当前路径切换到本 job 的临时工作路径
		 * @exception JobHandleException ，切换到临时工作路径失败
		 */
		void change_job_dir() const;

		/**
		 * @brief 将当前路径切换出本 job 的临时工作路径并清理该工作区
		 * @exception 该函数保证不抛出任何异常
		 */
		void clean_job_dir() const noexcept;

		int calculate_similarity();

		/**
		 * @brief 根据当前 config 配置的值，执行编译/运行程序。过程中会对必要的权限检查，
		 * 并且会启动一个线程来监控，防止运行超时。执行结束后返回执行结果。
		 * @param config 执行配置
		 * @return 根据执行结果而确定的 Result 结构体。包含资源使用信息与程序退出代码等。
		 * @exception 该函数保证不抛出任何异常
		 */		
		Result execute(const Config & config) const noexcept;

		/**
		 * @brief 进入当前 job 的编译流程，并返回编译结果。
		 * @return 编译的结果信息。包含资源使用信息与编译成功与否等。
		 * @exception 该函数保证不抛出任何异常
		 */	
		Result compile() const noexcept;

		/**
		 * @brief 当编译出错时，将编译错误信息写入到 redis。
		 * @return 若编译错误信息写入成功，返回 true ，否则返回 false
		 * @exception 该函数保证不抛出任何异常
		 */	
		bool set_compile_info() noexcept;

		/**
		 * @brief 进入当前 job 的执行流程，并返回执行结果。
		 * @return 执行的结果信息。包含资源使用信息与执行成功与否等。
		 * @exception 该函数保证不抛出任何异常
		 */	
		Result running();

		/**
		 * @brief 启动 编译/执行 的实际工作。限制了资源信息，重定向流之后，启动实际工作进程。
		 * @return 若该函数执行正常，返回值由取代此子进程的实际工作进程返回值决定，否则返回 false
		 * @note 若限制资源信息、重定向流等信息失败，会产生用户自定义信号 SIGUSR1 ，用于调用函数判定系统出错
		 */			
		bool child_process(const Config & config) const;

		/**
		 * @brief 将评测结果提交到 redis 数据库。
		 */		
		void commitJudgeResultToRedis(const SolutionDetails & result);

		/**
		 * @brief 将查重结果提交到 redis 数据库。
		 */	
		void commit_simtxt_to_redis();

	public:
		/**
		 * @brief 将评测失败的评测详情插入数据库、将评测失败的 job_item 插入评测失败队列
		 * @exception 该函数保证不抛出任何异常
		 * @note 该函数实际调用了 insert_into_failed 静态函数。是给 JudgeJob 对象创建成功后使用。若创建对象失败则不可使用此函数。
		 */	
		bool push_back_failed_judge_job() noexcept;

		/**
		 * @brief 将评测失败的评测详情插入数据库、将评测失败的 job_item 插入评测失败队列
		 * @exception 该函数保证不抛出任何异常
		 * @note 该函数为静态成员函数。无论对象创建是否成功均可使用，保证了逻辑的通用性。
		 */	
		static bool insert_into_failed(const kerbal::redis::RedisContext & conn, int jobType, int sid) noexcept;
};


#endif /* SRC_JOBINFO_HPP_ */
