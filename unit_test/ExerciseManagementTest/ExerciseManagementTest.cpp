/*
 * ExerciseManagementTest.cpp
 *
 *  Created on: 2018年11月12日
 *      Author: peter
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <fstream>

using namespace std;

#include <kerbal/range/range.hpp>

#include <boost/test/minimal.hpp>

#include "logger.cpp"
#include "ExerciseManagement.cpp"
#include "sync_instance_pool.hpp"

typedef sync_instance_pool<mysqlpp::Connection> conn_pool;


std::ofstream log_fp("/dev/null");

int test_main(int argc, char *argv[])
// 测试主函数，不需要在定义main()
{
	try {
		using namespace boost;

		for(int i = 0; i < 130; ++i) {
			mysqlpp::Connection conn(false);
			conn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
			if(!conn.connect("ojv4", "127.0.0.1", "root", "123")) {
				throw std::runtime_error("connected failed!");
			}
			conn_pool::construct(1, std::move(conn));
		}

		ExerciseManagement::refresh_all_problems_submit_and_accept_num();
		LOG_INFO(0, 0, log_fp, "refresh_all_user_problem finished!");

	} catch (const std::exception & e) {
		EXCEPT_FATAL(0, 0, log_fp, "refresh_all_user_problem falied!", e);
		BOOST_FAIL(e.what());            // 给出一个错误信息，终止执行
	} catch (...) {
		BOOST_FAIL("致命错误，测试终止");            // 给出一个错误信息，终止执行
	}

	return 0;
}
