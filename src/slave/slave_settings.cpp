/*
 * slave_settings.cpp
 *
 *  Created on: 2019年3月9日
 *      Author: peter
 */

#include <iostream>
#include "slave_settings.hpp"

void Settings::parse(const boost::filesystem::path & config_file)
{
	nlohmann::json json_obj;
	{
		std::ifstream config_file_stream {config_file};
		config_file_stream >> json_obj;
		std::cout << json_obj.dump(4) << std::endl;
	}

	{
		const auto & runtime_node = json_obj.at("runtime");
		runtime.log_file_path = std::string(runtime_node.at("log_file_path"));
		runtime.lock_file_path = std::string(runtime_node.at("lock_file_path"));
	}

	{
		const auto & judge_node = json_obj.at("judge");
		judge.workspace_dir = std::string(judge_node.at("workspace_dir"));
		judge.input_dir = std::string(judge_node.at("input_dir"));
		judge.judger_uid = judge_node.at("judger_uid");
		judge.judger_gid = judge_node.at("judger_gid");
		judge.max_cpu_consuming_task_num = judge_node.at("max_cpu_consuming_task_num");

		{
			const auto & java_node = judge_node.at("java");
			judge.java.policy_path = std::string(java_node.at("policy_path"));
			judge.java.memory_bonus = kerbal::utility::MB{java_node.at("memory_bonus")};
		}

		{
			const auto & similarity_test_node = judge_node.at("similarity_test");
			judge.similarity_test.accepted_solutions_dir = std::string(similarity_test_node.at("accepted_solutions_dir"));
			judge.similarity_test.stored_ac_code_max_num = similarity_test_node.at("stored_ac_code_max_num");
		}
	}

	{
		const auto & redis_node = json_obj.at("redis");
		redis.hostname = redis_node.at("hostname");
		redis.port = redis_node.at("port");
	}
}

Settings __settings;

std::reference_wrapper<const Settings> settings(__settings);

