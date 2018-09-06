#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "logger.hpp"

#include <time.h>

#include "logger.hpp"
#include "logger.cpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>

using namespace std;

std::ofstream log_fp("/dev/null");

int main()
{
	int jobType = 0;
	int sid = 100;
	
	mysqlpp::Connection mysqlConn("ojv4", "127.0.0.1", "root" , "123");
	
	mysqlpp::Query query = mysqlConn.query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id %2 and status = 0"
    );
    query.parse();
    

    mysqlpp::StoreQueryResult res;
    try {
        res = query.store(1, 1001, "is null");
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error information: ", e.what());
        throw;
    }

    cout << res.size() << endl;
}











