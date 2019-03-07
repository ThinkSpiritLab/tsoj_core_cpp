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
{
	try {

		for(int i = 0; i < 20; ++i) {
			mysqlpp::Connection conn(false);
			conn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
			if(!conn.connect("ojv4", "127.0.0.1", "root", "123")) {
				throw std::runtime_error("connected failed!");
			}
			conn_pool::construct(1, std::move(conn));
		}


//		std::deque<std::thread> th_group;
//
//		th_group.push_back(std::thread([](){
//			auto conn_handle = conn_pool::block_fetch();
//			auto conn = *conn_handle;
//			ExerciseManagement::refresh_all_users_submit_and_accept_num(conn);
//		}));
//
//		th_group.push_back(std::thread([](){
//			auto conn_handle = conn_pool::block_fetch();
//			auto conn = *conn_handle;
//			ExerciseManagement::refresh_all_problems_submit_and_accept_num(conn);
//		}));

		auto conn_handle = conn_pool::block_fetch();
		auto conn = *conn_handle;
		mysqlpp::Query query = conn.query(
			"select distinct c_id from course"
		);

		ExerciseManagement::refresh_all_user_problem2(conn);

//		mysqlpp::StoreQueryResult res = query.store();
//
//		for (const auto & row : res) {
//			ojv4::c_id_type c_id = row["c_id"];
//			th_group.push_back(std::thread([c_id]() {
//				cout << "update course user: " << c_id << endl;
//				auto conn_handle = conn_pool::block_fetch();
//				auto conn = *conn_handle;
//				CourseManagement::refresh_all_problems_submit_and_accept_num_in_course(conn, c_id);
//			}));
//			th_group.push_back(std::thread([c_id]() {
//				cout << "update course problem: " << c_id << endl;
//				auto conn_handle = conn_pool::block_fetch();
//				auto conn = *conn_handle;
//				CourseManagement::refresh_all_users_submit_and_accept_num_in_course(conn, c_id);
//			}));
//		}
//
//		for (auto & t : th_group) {
//			t.join();
//		}

	} catch (const std::exception & e) {
		cerr << e.what() << endl;            // 给出一个错误信息，终止执行
	} catch (...) {
		cerr << "致命错误" << endl;            // 给出一个错误信息，终止执行
	}

	return 0;
}

