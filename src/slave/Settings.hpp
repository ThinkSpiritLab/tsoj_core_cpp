/*
 * Settings.hpp
 *  Since: 2018/12/18
 *  Source from: global_shared_variable.hpp created on 2018/6/15
 *      Author: peter
 */

#ifndef SRC_SLAVE_SETTINGS_HPP_
#define SRC_SLAVE_SETTINGS_HPP_

#include "sync_nonsingle_instance_pool.hpp"

#include <fstream>
#include <string>

#include <kerbal/redis_v2/connection.hpp>
#include <boost/filesystem.hpp>

#define DEF_PROTECTED_FIELD(filed_type, filed_name) \
protected: \
	static inline filed_type filed_name##_v; \
public: \
	static const filed_type & filed_name() \
	{ \
		return filed_name##_v; \
	} \
private:

class Settings
{
	public:
		friend void load_config(const char * config_file);
		static inline std::ofstream log_fp; ///< 日志文件的文件流
		static inline boost::filesystem::path lock_file; ///< lock_file, 用于保证一次只能有一个 judge_server 守护进程在运行

		class JudgerRuntimeVariable
		{
				friend void load_config(const char * config_file);

				/// 本机主机名
				DEF_PROTECTED_FIELD(std::string, host_name);

				/// 本机 IP
				DEF_PROTECTED_FIELD(std::string, ip);

				/// 本机当前用户名
				DEF_PROTECTED_FIELD(std::string, user_name);

				/// 本机监听进程号
				DEF_PROTECTED_FIELD(std::string, listening_pid);

				/// 评测服务器id，定义为 host_name:ip
				DEF_PROTECTED_FIELD(std::string, judge_server_id);
		};

		class JudgeSettings
		{
				friend void load_config(const char * config_file);

				/// 程序的运行目录，用于存放临时文件等
				DEF_PROTECTED_FIELD(boost::filesystem::path, init_dir)

				/// 测试数据和答案的存放目录
				DEF_PROTECTED_FIELD(boost::filesystem::path, input_dir)

				/// Linux 中运行评测进程的 user id. 默认为 1666
				DEF_PROTECTED_FIELD(int, judger_uid)

				/// Linux 中运行评测进程的 group id. 默认为 1666
				DEF_PROTECTED_FIELD(int, judger_gid)

				/// java 运行策略配置文件路径
				DEF_PROTECTED_FIELD(boost::filesystem::path, java_policy_path)

				/// java 额外运行时间值
				DEF_PROTECTED_FIELD(ojv4::s_time_in_milliseconds, java_time_bonus)

				/// Linux 中运行评测进程的 user id. 默认为 1666
				DEF_PROTECTED_FIELD(ojv4::s_mem_in_byte, java_memory_bonus)

				/// 最大允许同时进行的任务数
				DEF_PROTECTED_FIELD(int, max_running)
		};

		class RedisSettings
		{
				friend void load_config(const char * config_file);

				/// redis 主机名
				DEF_PROTECTED_FIELD(std::string, redis_hostname)

				/// redis 端口号
				DEF_PROTECTED_FIELD(int, redis_port)

				/// redis 最大连接数
				DEF_PROTECTED_FIELD(int, max_redis_conn)
		};

//		static void load(const char * config_file)
//		{
//
//		}
};

inline sync_nonsingle_instance_pool<kerbal::redis_v2::connection> redis_pool;

#endif /* SRC_SLAVE_SETTINGS_HPP_ */
