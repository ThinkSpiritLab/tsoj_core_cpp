/*
 * ContestManagementTest.cpp
 *
 *  Created on: 2018年11月10日
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

#include "ContestManagement.cpp"
#include "logger.cpp"
#include "sync_instance_pool.hpp"

typedef sync_instance_pool<mysqlpp::Connection> conn_pool;

//void update_problem_submit_and_accept_num(conn_pool::auto_revert_t conn, int ct_id, int p_id)
//{
//	ContestManagement::update_problem_submit_and_accept_num(*conn, 1, p_id);
//}


std::ofstream log_fp("/dev/null");

int test_main(int argc, char *argv[])
// 测试主函数，不需要在定义main()
{
	try {
		using namespace boost;

		conn_pool::construct(64, "ojv4", "127.0.0.1", "root", "123");

		ContestManagement::refresh_all_problems_submit_and_accept_num_in_contest(1);

	} catch (const std::exception & e) {
		BOOST_FAIL(e.what());            // 给出一个错误信息，终止执行
	} catch (...) {
		BOOST_FAIL("致命错误，测试终止");            // 给出一个错误信息，终止执行
	}

	return 0;
}

