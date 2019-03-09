/*
 * master_settings.cpp
 *
 *  Created on: 2019年3月9日
 *      Author: peter
 */

#include <iostream>
#include "master_settings.hpp"

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
	}

	{
		const auto & mysql_node = json_obj.at("mysql");
		mysql.hostname = mysql_node.at("hostname");
		mysql.username = mysql_node.at("username");
		mysql.passward = mysql_node.at("passward");
		mysql.database = mysql_node.at("database");
		mysql.port = mysql_node.at("port");
	}

	{
		const auto & redis_node = json_obj.at("redis");
		redis.hostname = redis_node.at("hostname");
		redis.port = redis_node.at("port");
	}
}

Settings __settings;

std::reference_wrapper<const Settings> settings(__settings);


