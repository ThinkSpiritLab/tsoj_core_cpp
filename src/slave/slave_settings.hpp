/*
 * slave_settings.hpp
 *
 *  Created on: 2019年3月9日
 *      Author: peter
 */

#ifndef SRC_SLAVE_SLAVE_SETTINGS_HPP_
#define SRC_SLAVE_SLAVE_SETTINGS_HPP_

#include <fstream>
#include <boost/filesystem/path.hpp>
#include <nlohmann/json.hpp>

#include "db_typedef.hpp"

class Settings
{
	public:

		struct
		{
				boost::filesystem::path log_file_path;
				boost::filesystem::path lock_file_path;
		} runtime;

		struct
		{
				boost::filesystem::path workspace_dir;
				boost::filesystem::path input_dir;
				int judger_uid;
				int judger_gid;
				int max_cpu_consuming_task_num;

				struct
				{
						boost::filesystem::path policy_path;
						ojv4::s_mem_in_byte memory_bonus;
				} java;

				struct
				{
						boost::filesystem::path accepted_solutions_dir;
						int stored_ac_code_max_num;
				} similarity_test;
		} judge;

		struct
		{
				std::string hostname;
				int port;
		} redis;

		void parse(const boost::filesystem::path & config_file);
};

extern std::reference_wrapper<const Settings> settings;

#endif /* SRC_SLAVE_SLAVE_SETTINGS_HPP_ */
