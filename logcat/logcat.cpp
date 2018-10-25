/*
 * logcat.cpp
 *
 * Author:ZhangBinjie@Penguin
 * Created on: 2018年10月23日
 */

#include <memory>
#include "config.hpp"
#include "config_exception.hpp"
#include "taskbase.hpp"
#include "task_exception.hpp"

int main(const int argc, const char * argv[]) try {
	Config config;
	try {
		config.load_parameters(argc, argv);
	} catch (ConfigException &e) {
		std::cout << e.what() << std::endl;
		std::cout << "please check the manual for help by 'logcat --help' or 'logcat -H'." << std::endl;
		exit(0);
	}

	std::unique_ptr<TaskBase> task = nullptr;
	try {
		task = make_task(config);
		task->handle();
	} catch (TaskException &e) {
		std::cout << e.what() << std::endl;
		std::cout << "please check the manual for help by 'logcat --help' or 'logcat -H'." << std::endl;
		exit(0);		
	}

	exit(0);
} catch (const std::exception & e) {
	std::cout << "An uncaught exception caught by main. The info is: " << e.what() << std::endl;
	std::cout << "Please turn to author." << std::endl;
	exit(0);
} catch (...) {
	std::cout << "An uncaught exception caught by main." << std::endl;
	std::cout << "Please turn to author." << std::endl;
	exit(0);
}