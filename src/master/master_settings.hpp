/*
 * master_settings.hpp
 *
 *  Created on: 2019年3月9日
 *      Author: peter
 */

#ifndef SRC_MASTER_MASTER_SETTINGS_HPP_
#define SRC_MASTER_MASTER_SETTINGS_HPP_

#include <fstream>
#include <boost/filesystem/path.hpp>
#include <nlohmann/json.hpp>

class Settings
{
	public:

		struct
		{
				boost::filesystem::path log_file_path;
		} runtime;

		struct
		{
				std::string hostname;
				std::string username;
				std::string passward;
				std::string database;
				int port;
		} mysql;

		struct
		{
				std::string hostname;
				int port;
		} redis;

		void parse(const boost::filesystem::path & config_file);
};

extern std::reference_wrapper<const Settings> settings;


#endif /* SRC_MASTER_MASTER_SETTINGS_HPP_ */
