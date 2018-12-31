/*
 * sync.cpp
 *
 *  Created on: 2018年12月31日
 *      Author: peter
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <fstream>

using namespace std;

#include <kerbal/range/range.hpp>

#include "CourseManagement.cpp"
#include "ExerciseManagement.cpp"
#include "logger.cpp"
#include "sync_instance_pool.hpp"

typedef sync_instance_pool<mysqlpp::Connection> conn_pool;


std::ofstream log_fp("/dev/null");

int main(int argc, char *argv[])
// 测试主函数，不需要在定义main()
{
	try {

		mysqlpp::Connection conn(false);
		conn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
		conn.connect("ojv4", "127.0.0.1", "root", "123");

		cout << "update exercise" << endl;
		ExerciseManagement::refresh_all_users_submit_and_accept_num(conn);
		ExerciseManagement::refresh_all_problems_submit_and_accept_num(conn);
/*
		mysqlpp::Query query = conn.query(
			"select distinct c_id from course"
		);

		mysqlpp::StoreQueryResult res = query.store();

		for(const auto & row : res) {
			ojv4::c_id_type c_id = row["c_id"];
			cout << "update course: " << c_id << endl;
			CourseManagement::refresh_all_problems_submit_and_accept_num_in_course(conn, c_id);
			CourseManagement::refresh_all_users_submit_and_accept_num_in_course(conn, c_id);
		}
*/
	} catch (const std::exception & e) {
		cerr << e.what() << endl;            // 给出一个错误信息，终止执行
	} catch (...) {
		cerr << "致命错误" << endl;            // 给出一个错误信息，终止执行
	}

	return 0;
}

