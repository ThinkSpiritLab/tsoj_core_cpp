/*
 * UpdatorJob.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#include "../master_old/UpdatorJob.hpp"

#include "boost_format_suffix.hpp"

#include "logger.hpp"

#include <dirent.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <algorithm>

extern std::string accepted_solutions_dir;
extern std::fstream log_fp;
extern int stored_ac_code_max_num;

void UpdatorJob::fetchDetailsFromRedis()
{
    supper_t::fetchDetailsFromRedis();

    using std::stoi;
    static boost::format key_name_tmpl("job_info:%d:%d");

    std::vector<std::string> query_res;

    try {
        Operation opt(redisConn);
        query_res = opt.hmget((key_name_tmpl % jobType % sid).str(), "uid", "cid", "post_time", "have_accepted",
                              "no_store_ac_code", "is_rejudge");
    } catch (const RedisNilException & e) {
        LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
        throw;
    } catch (const RedisUnexpectedCaseException & e) {
        LOG_FATAL(0, sid, log_fp, "redis returns an unexpected type. Exception infomation: ", e.what());
        throw;
    } catch (const RedisException & e) {
        LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
        throw;
    } catch (const std::exception & e) {
        LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", e.what());
        throw;
    } catch (...) {
        LOG_FATAL(0, sid, log_fp, "job doesn't exist. Exception infomation: ", UNKNOWN_EXCEPTION_WHAT);
        throw;
    }

    try {
        this->uid = query_res[0];
        this->cid = stoi(query_res[1]);
        this->postTime = query_res[2];
        this->haveAccepted = stoi(query_res[3]);
        this->no_store_ac_code = (bool) stoi(query_res[4]);
        this->is_rejudge = (bool) stoi(query_res[5]);
    } catch (const std::exception & e) {
        LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ", e.what());
        throw;
    } catch (...) {
        LOG_FATAL(0, sid, log_fp, "job details lost or type cast failed. Exception infomation: ",
                  UNKNOWN_EXCEPTION_WHAT);
        throw;
    }
}

int UpdatorJob::calculate_similarity() const
{
    std::string sim_cmd;
    switch (this->lang) {
        case Language::C:
            sim_cmd = "sim_c -S -P Main.c / %s/%d/0/* > sim.txt"_fmt(accepted_solutions_dir, this->pid).str();
            break;
        case Language::Cpp:
        case Language::Cpp14:
            sim_cmd = "sim_c -S -P Main.cpp / %s/%d/0/* > sim.txt"_fmt(accepted_solutions_dir, this->pid).str();
            break;
        case Language::Java:
            sim_cmd = "sim_java -S -P Main.java / %s/%d/2/* > sim.txt"_fmt(accepted_solutions_dir, this->pid).str();
            break;
    }

    int sim_cmd_return_value = system(sim_cmd.c_str());
    if (-1 == sim_cmd_return_value) {
        throw JobHandleException("sim execute failed");
    }
    if (!WIFEXITED(sim_cmd_return_value) || WEXITSTATUS(sim_cmd_return_value)) {
        throw JobHandleException(
                "sim command exits incorrectly, exit status: " + std::to_string(WEXITSTATUS(sim_cmd_return_value)));
    }

    std::ifstream fp("sim.txt", std::ios::in);

    if (!fp) {
        throw JobHandleException("sim.txt open failed");
    }

    std::string buf;
    while (std::getline(fp, buf)) {
        if (buf.empty()) {
            break;
        }
    }

    int max_similarity = 0;
    while (std::getline(fp, buf)) {
        std::string::reverse_iterator en = std::find(buf.rbegin(), buf.rend(), '%');
        if (en != buf.rend()) {
            do {
                ++en;
            } while (std::isspace(*en));
            std::string::reverse_iterator be = std::find_if_not(en, buf.rend(), ::isdigit);
            --en;
            --be;
            try {
                int now_similarity = std::stoi(
                        buf.substr(buf.length() - (std::distance(buf.rbegin(), be)) - 1, std::distance(en, be)));
                max_similarity = std::max(max_similarity, now_similarity);
            } catch (const std::invalid_argument & e) {
                throw JobHandleException("an error occurred while calculate max similarity");
            }
        }
    }

    return max_similarity;
}


void UpdatorJob::store_code_to_accepted_solutions_dir() const
{
// 将已经AC且no_store_ac_code标志不为1的代码存入accepted_solutions_dir目录
    std::string code_path = accepted_solutions_dir;

    // 以下逐个检查文件夹可写权限
    if (access(code_path.c_str(), W_OK) != 0) {
        LOG_FATAL(jobType, sid, log_fp, "accepted_solutions_dir does not have write permission");
        throw JobHandleException("accepted_solutions_dir does not have write permission");
    }
    code_path += "/" + std::to_string(pid);
    if (access(code_path.c_str(), W_OK) != 0) {
        if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            LOG_FATAL(jobType, sid, log_fp, "Cannot create directory: ", code_path);
            throw JobHandleException("Cannot create directory");
        }
    }
    switch (lang) {
        case Language::C:
        case Language::Cpp:
        case Language::Cpp14:
            code_path += "/0";
            break;
        case Language::Java:
            code_path += "/2";
            break;
    }
    if (access(code_path.c_str(), W_OK) != 0) {
        if (mkdir(code_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            LOG_FATAL(jobType, sid, log_fp, "Cannot create directory: ", code_path);
            throw JobHandleException("Cannot create directory");
        }
    }

    std::vector<int> arr;
    arr.reserve(stored_ac_code_max_num);

    std::unique_ptr<DIR, int (*)(DIR *)> dir(opendir(code_path.c_str()), closedir);
    if (dir == nullptr) {
        LOG_FATAL(jobType, sid, log_fp, "Cannot open directory ", code_path,
                  " for checking the number of accepted code");
        throw JobHandleException("Cannot open directory for checking the number of accepted code");
    }

    for (const dirent * ptr = nullptr; ptr = readdir(dir.get()), ptr != nullptr;) {
        if (strcmp(".", ptr->d_name) != 0 && strcmp("..", ptr->d_name) != 0) {
            try {
                arr.push_back(std::stoi(ptr->d_name));
            } catch (...) {
            }
        }
    }
    dir = nullptr; //关闭目录 (其实不加这句出了作用域也会自动关的, 在这写一遍的目的是为了提醒)

    // 检查本题已经保存的AC代码份数是否超过了stored_ac_code_max_num
    if (arr.size() > stored_ac_code_max_num) {

        std::partial_sort(arr.begin(), arr.end() - stored_ac_code_max_num, arr.end());

        for (std::vector<int>::iterator it = arr.begin(); it != arr.end() - stored_ac_code_max_num; ++it) {
            switch (lang) {
                case Language::C:
                case Language::Cpp:
                case Language::Cpp14: {
                    std::string file_path = "%s/%d.c"_fmt(code_path, *it).str();
                    unlink(file_path.c_str());
                    file_path = "%s/%d.cpp"_fmt(code_path, *it).str();
                    unlink(file_path.c_str());
                    break;
                }
                case Language::Java: {
                    std::string file_path = "%s/%d.java"_fmt(code_path, *it).str();
                    unlink(file_path.c_str());
                    break;
                }
            }
        }
    }

    // 开始复制AC代码
    std::string copy_cmd;
    switch (lang) {
        case Language::C:
            copy_cmd = "cp Main.c %s/%d.c"_fmt(code_path, sid).str();
            break;
        case Language::Cpp:
        case Language::Cpp14:
            copy_cmd = "cp Main.cpp %s/%d.cpp"_fmt(code_path, sid).str();
            break;
        case Language::Java:
            copy_cmd = "cp Main.java %s/%d.java"_fmt(code_path, sid).str();
            break;
    }

    int copy_cmd_return_value = system(copy_cmd.c_str());
    // 检查命令执行是否成功
    if (-1 == copy_cmd_return_value) {
        LOG_FATAL(jobType, sid, log_fp, "Fork failed while copying code to accepted_solutions_dir");
        throw JobHandleException("Fork failed while copying code to accepted_solutions_dir");
    }
    if (!WIFEXITED(copy_cmd_return_value) || WEXITSTATUS(copy_cmd_return_value)) {
        LOG_FATAL(jobType, sid, log_fp, "Error occurred while copying code to accepted_solutions_dir, exit status: ",
                  WEXITSTATUS(copy_cmd_return_value));
        throw JobHandleException("Error occurred while copying code to accepted_solutions_dir");
    }
}

Result UpdatorJob::get_job_result()
{
    try {
        std::vector<std::string> query_res;
        Result res;
        static boost::format judge_status("judge_status:%d:%d");
        Operation opt(redisConn);
        query_res = opt.hmget((judge_status % jobType % sid).str(), "result", "cpu_time", "memory");
        res.judge_result = std::stoi(query_res[0]);
        res.cpu_time = std::chrono::milliseconds{std::stoull(query_res[1])};
        res.memory = kerbal::utility::Byte{std::stoull(query_res[2])};
        return res;
    } catch (const RedisUnexpectedCaseException & e) {
        LOG_FATAL(0, sid, log_fp, "redis returns an unexpected type. Exception infomation: ", e.what());
        throw;
    } catch (const RedisException & e) {
        LOG_FATAL(0, sid, log_fp, "job result doesn't exist. Exception infomation: ", e.what());
        throw;
    } catch (const std::exception & e) {
        LOG_FATAL(0, sid, log_fp, "job result doesn't exist. Exception infomation: ", e.what());
        throw;
    } catch (...) {
        LOG_FATAL(0, sid, log_fp, "job result doesn't exist. Exception infomation: ", "unknown exception");
        throw;
    }
}

void UpdatorJob::set_cheat_status(std::chrono::seconds expire_time) const
{
    Operation opt(redisConn);
    static boost::format cheat_status("cheat_status:%d");
    std::string cheat_status_key = (cheat_status % uid).str();
    opt.set(cheat_status_key, pid);
    opt.expire(cheat_status_key, expire_time);
}


#include <stdio.h>
#include <cstring>


enum user_problem_status
{
    // ACCEPTED = 0, defined in compare_result
            ATTEMPTED = 1,
};


void UpdatorJob::update_solution(const Result & result)
{
    try {
        if (jobType == 0) {
            if (cid) {
                this->update_course_solution(result);
            } else {
                this->update_exercise_solution(result);
            }
        } else {
        	this->update_contest_solution(result);
        }
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution failed! Error information: ", e.what());
        throw;
    } catch (const MysqlEmptyResException & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution failed! Error information: ", e.what(), " error code: ", e.errnum());
        throw;
    } catch (const std::exception & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution failed! Error information: ", e.what());
        throw;
    }
}

void UpdatorJob::update_exercise_solution(const Result & result)
{
	std::string solution_table = "solution";
	mysqlpp::Query templ = mysqlConn.query(
			"insert into %0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
	);
	templ.parse();
	mysqlpp::SimpleResult res = templ.execute(solution_table, sid, uid, pid, (int) lang, result.judge_result,
									 result.cpu_time.count(), result.memory.count(), postTime,
									 result.similarity_percentage);
	if (!res) {
		throw MysqlEmptyResException(templ.errnum(), templ.error());
	}
}

void UpdatorJob::update_course_solution(const Result & result)
{
	std::string solution_table = "solution";
	mysqlpp::Query templ = mysqlConn.query(
			"insert into %0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, c_id, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9, %10)"
	);
	templ.parse();
	mysqlpp::SimpleResult res = templ.execute(solution_table, sid, uid, pid, (int) lang, result.judge_result,
								   result.cpu_time.count(), result.memory.count(), postTime, cid,
								   result.similarity_percentage);
    if (!res) {
        throw MysqlEmptyResException(templ.errnum(), templ.error());
    }
}

void UpdatorJob::update_contest_solution(const Result & result)
{
	mysqlpp::Query templ = mysqlConn.query(
			"insert into %0 "
			"(s_id, u_id, p_id, s_lang, s_result, s_time, s_mem, s_posttime, s_similarity_percentage)"
			"values (%1, %2, %3, %4, %5, %6, %7, %8q, %9)"
	);
	templ.parse();
	std::string solution_table = "contest_solution%d"_fmt(jobType).str();
	mysqlpp::SimpleResult res = templ.execute(solution_table, sid, uid, pid, (int) lang, result.judge_result,
									result.cpu_time.count(), result.memory.count(), postTime,
									result.similarity_percentage);
    if (!res) {
        throw MysqlEmptyResException(templ.errnum(), templ.error());
    }
}

void UpdatorJob::update_source_code(const char * source_code)
{
    std::string source_code_table;
    if (jobType == 0) {
        source_code_table = "source";
    } else {
        source_code_table = "contest_source%d"_fmt(jobType).str();
    }
    mysqlpp::Query query = mysqlConn.query(
            "insert into %0 (s_id, source_code)"
            "values (%1, %2q)"
    );
    query.parse();
    mysqlpp::SimpleResult res = query.execute(source_code_table, sid, source_code);
    if (!res) {
        throw MysqlEmptyResException(query.errnum(),query.error());
    }
}

void UpdatorJob::update_compile_info()
{
    static RedisCommand get_ce_info_templ("get compile_info:%d:%d");
    RedisReply reply = get_ce_info_templ.execute(redisConn, jobType, sid);
    if (reply.replyType() != RedisReplyType::STRING) {
        LOG_FATAL(this->jobType, this->sid, log_fp, "Unable to get compile info!");
        throw kerbal::redis::RedisUnexpectedCaseException(reply.replyType());
    }

    if (std::strcmp(reply->str, "") == 0) {
        LOG_WARNING(jobType, sid, log_fp, "Empty compile info string!");
    }

    std::string compile_info_table;
    if (jobType == 0) {
        compile_info_table = "compile_info";
    } else {
        compile_info_table = "contest_compile_info%d"_fmt(jobType).str();
    }

    mysqlpp::Query insert = mysqlConn.query(
            "insert into %0 (s_id, compile_error_info) values (%1, %2q)"
    );
    insert.parse();
    mysqlpp::SimpleResult res;
    try {
        res = insert.execute(compile_info_table, sid, reply->str);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update compile_info failed! Error information: ", e.what());
        throw;
    }
    if (!res) {
        LOG_FATAL(jobType, sid, log_fp, "Update compile_info failed! ",
                  "Error code: ", insert.errnum(),
                  ", Error information: ", insert.error());
        throw std::logic_error(insert.error());
    }
}

int UpdatorJob::update_user_and_problem(const Result & result)
{
    std::string cid_param;
    if (cid > 0) {
        cid_param = "= %d"_fmt(cid).str();
    } else {
        cid_param = "is NULL";
    }
    mysqlpp::Query query = mysqlConn.query(
            "select status from user_problem "
            "where u_id = %0 and p_id = %1 and c_id %2 and status = 0"
    );
    mysqlpp::StoreQueryResult res;
    bool already_AC = false;
    try {
        res = query.store(uid, pid, cid_param);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! Error information: ", e.what());
        throw;
    }
    if (res.empty()) {
        if (query.errnum() != 0) {
            LOG_FATAL(jobType, sid, log_fp, "Select status from user_problem failed! ",
                      "Error code: ", query.errnum(),
                      ", Error information: ", query.error());
            throw std::logic_error(query.error());
        }
    } else {
    	already_AC = true;
    }

    // AC后再提交不增加submit数
    if (result.judge_result == UnitedJudgeResult::ACCEPTED && already_AC == false) {
        {
            mysqlpp::Query update = mysqlConn.query(
                    "update user set u_accept = u_accept + 1, u_submit = u_submit + 1 where u_id = %0"
            );
            update.parse();
            mysqlpp::SimpleResult res;
            try {
                res = update.execute(uid);
            } catch (const mysqlpp::BadParamCount & e) {
                LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
                throw;
            }
            if (!res) {
                LOG_FATAL(jobType, sid, log_fp, "Update user failed! ",
                          "Error code: ", update.errnum(),
                          ", Error information: ", update.error());
                throw std::logic_error(update.error());
            }
        }
        {
            mysqlpp::Query update = mysqlConn.query(
                    "update problem set p_accept = p_accept + 1, p_submit = p_submit + 1 where p_id = %0"
            );
            update.parse();
            mysqlpp::SimpleResult res;
            try {
                res = update.execute(pid);
            } catch (const mysqlpp::BadParamCount & e) {
                LOG_WARNING(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
                throw;
            }
            if (!res) {
                LOG_WARNING(jobType, sid, log_fp, "Update problem failed! ",
                            "Error code: ", update.errnum(),
                            ", Error information: ", update.error());
                throw std::logic_error(update.error());
            }
        }
    } else if (already_AC == false) {
    	{
			mysqlpp::Query update = mysqlConn.query(
					"update user set u_submit = u_submit + 1 where u_id = %0"
			);
			update.parse();
			mysqlpp::SimpleResult res;
			try {
				res = update.execute(uid);
			} catch (const mysqlpp::BadParamCount & e) {
				LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
				throw;
			}
			if (!res) {
				LOG_FATAL(jobType, sid, log_fp, "Update user failed! ",
						  "Error code: ", update.errnum(),
						  ", Error information: ", update.error());
				throw std::logic_error(update.error());
			}
		}
		{
			mysqlpp::Query update = mysqlConn.query(
					"update problem set p_submit = p_submit + 1 where p_id = %0"
			);
			update.parse();
			mysqlpp::SimpleResult res;
			try {
				res = update.execute(pid);
			} catch (const mysqlpp::BadParamCount & e) {
				LOG_WARNING(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
				throw;
			}
			if (!res) {
				LOG_WARNING(jobType, sid, log_fp, "Update problem failed! ",
							"Error code: ", update.errnum(),
							", Error information: ", update.error());
				throw std::logic_error(update.error());
			}
		}
    }

    return 0;
}

int UpdatorJob::update_course_user(const Result & result)
{
    MYSQL_RES * res;
    MYSQL_ROW row;
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select status from user_problem "
                 "where u_id= %d and p_id = %d and c_id = %d and status = '0'", uid, pid, cid);
    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "select status from user_problem (course problem status) failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    int already_AC = 0;
    row = mysql_fetch_row(res);
    if (row != NULL) {
        already_AC = 1;
    }
    memset(sql, 0, sizeof(sql));
    mysql_free_result(res);
    //已经通过 再次提交不增加c_submit
    if (result.judge_result == UnitedJudgeResult::ACCEPTED && !already_AC) {
        sprintf(sql, "update course_user set c_accept = c_accept + 1, c_submit = c_submit + 1 "
                     "where u_id = %d and c_id = %d", uid, cid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp, "update course_user failed!,errno:%d, error:%s", mysql_errno(conn),
                        mysql_error(conn));
            return ret;
        }
    } else if (!already_AC) {
        sprintf(sql, "update course_user set c_submit = c_submit + 1 "
                     "where u_id = %d and c_id = %d", uid, cid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp, "update course_user failed!,errno:%d, error:%s", mysql_errno(conn),
                        mysql_error(conn));
            return ret;
        }
    }

    return 0;
}

int UpdatorJob::update_user_problem(int stat)
{
    MYSQL_RES * res;
    MYSQL_ROW row;
    memset(sql, 0, sizeof(sql));
    if (cid) {
        sprintf(sql, "select status from user_problem "
                     "where u_id = %d and p_id = %d and c_id = %d", uid, pid, cid);
    } else {
        sprintf(sql, "select status from user_problem "
                     "where u_id = %d and p_id = %d and c_id is NULL", uid, pid);
    }
    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "select status from user_problem failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    memset(sql, 0, sizeof(sql));
    if (row == NULL) {
        if (cid) {
            sprintf(sql, "insert into user_problem (u_id, p_id, c_id, status)"
                         "values ('%d', '%d', '%d', '%d')", uid, pid, cid, stat);
        } else {
            sprintf(sql, "insert into user_problem (u_id, p_id, status)"
                         "values ('%d', '%d', '%d')", uid, pid, stat);
        }
    } else if (!stat && atoi(row[0])) {
        if (cid) {
            sprintf(sql, "update user_problem set status = %d "
                         "where u_id = %d and p_id = %d and c_id = %d", stat, uid, pid, cid);
        } else {
            sprintf(sql, "update user_problem set status = %d "
                         "where u_id = %d and p_id = %d and c_id is NULL", stat, uid, pid);
        }
    } else {
        mysql_free_result(res);
        return 0;
    }
    mysql_free_result(res);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "update user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                    mysql_error(conn), sql);
    }
    return ret;
}


int UpdatorJob::update_contest_user_problem(int is_ac)
{
    MYSQL_RES * res;
    MYSQL_ROW row;

    // is first AC
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select first_ac_user from contest_problem where ct_id = %d and p_id = %d", jobType, pid);

    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "select first_ac_user from contest_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                    mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "store result failed! res is NULL, errno:%d, error:%s\nsql:%s",
                  mysql_errno(conn), mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    if (row == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "cannot find record accordingly, maybe that problem has been deleted.\nsql:%s",
                  sql);
        return -1;
    }
    int is_first_ac = (row[0] == NULL ? 1 : 0);
    mysql_free_result(res);

    // is already AC
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select is_ac from contest_user_problem%d where u_id = %d and p_id = %d", jobType, uid, pid);

    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "select is_ac from contest_user_problem%d failed!, errno:%d, error:%s\nsql:%s", jobType,
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "store result failed! res is NULL, errno:%d, error:%s\nsql:%s",
                  mysql_errno(conn), mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);

    if (row == NULL) {
        memset(sql, 0, sizeof(sql));
        int error_count;
        if (is_ac) {
            error_count = 0;
        } else {
            error_count = 1;
            is_first_ac = 0;
        }
        sprintf(sql, "insert into contest_user_problem%d (u_id, p_id, is_ac, ac_time, error_count, is_first_ac) "
                     "values ('%d', '%d', '%d', '%s', '%d', '%d')", jobType, uid, pid, is_ac, postTime.c_str(),
                error_count, is_first_ac);

    } else if (atoi(row[0]) == 0) {
        // get error_count
        MYSQL_RES * tmp_res;
        MYSQL_ROW tmp_row;
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "select error_count from contest_user_problem%d "
                     "where u_id = %d and p_id = %d", jobType, uid, pid);
        int tmp_ret = mysql_real_query(conn, sql, strlen(sql));
        if (tmp_ret) {
            LOG_WARNING(jobType, sid, log_fp, "select status from user_problem failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return tmp_ret;
        }
        tmp_res = mysql_store_result(conn);
        if (tmp_res == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                      mysql_error(conn), sql);
            return mysql_errno(conn);
        }
        tmp_row = mysql_fetch_row(tmp_res);
        if (tmp_row == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "impossible! get error_count failed!, errno:%d, error:%s\nsql:%s",
                      mysql_errno(conn), mysql_error(conn), sql);
            return -1;
        }
        int error_count = atoi(tmp_row[0]);
        mysql_free_result(tmp_res);

        if (!is_ac) {
            error_count++;
            is_first_ac = 0;
        }

        memset(sql, 0, sizeof(sql));
        sprintf(sql, "update contest_user_problem%d set is_ac = %d, ac_time = '%s', error_count = %d, is_first_ac = %d "
                     "where u_id = %d and p_id = %d", jobType, is_ac, postTime.c_str(), error_count, is_first_ac, uid,
                pid);
    } else {
        mysql_free_result(res);
        return 0;
    }

    mysql_free_result(res);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "update contest_user_problem%d failed!, errno:%d, error:%s\nsql:%s", jobType,
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }

    // update first_ac_user
    if (is_ac && is_first_ac) {
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "update contest_problem set first_ac_user = %d where ct_id = %d and p_id = %d", uid, jobType, pid);

        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp, "update first_ac_user failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return ret;
        }
    }

    return 0;
}


int UpdatorJob::core_update_failed_table(const Result & result) noexcept
{
    RedisCommand get_src_code_templ("hget source_code:%d:%d source");
    RedisReply reply = get_src_code_templ.execute(redisConn, this->jobType, this->sid);
    const char * source_code = "";
    if (reply->type == REDIS_REPLY_STRING) {
        source_code = reply->str;
    }

    RedisCommand get_ce_info_templ("get compile_info:%d:%d");
    RedisReply reply2 = get_ce_info_templ.execute(redisConn, this->jobType, this->sid);
    const char * compile_error_info = "";
    if (reply2->type == REDIS_REPLY_STRING) {
        compile_error_info = reply2->str;
    }

    static char escape_compile_error_info[FILE_SIZE];
    memset(sql, 0, sizeof(sql));
    memset(escape_code, 0, sizeof(escape_code));
    memset(escape_compile_error_info, 0, sizeof(escape_compile_error_info));
    mysql_real_escape_string(conn, escape_code, source_code, strlen(source_code));
    mysql_real_escape_string(conn, escape_compile_error_info, compile_error_info, strlen(compile_error_info));
    sprintf(sql,
            "insert into core_update_failed (type, job_id, pid, uid, lang, post_time, cid, cases, time_limit, memory_limit, sim_threshold, result, cpu_time, memory, sim_percentage, source_code, compile_error_info) "
            "values ('%d','%d','%d','%d','%d','%s','%d','%d','%d','%d','%d','%d','%d','%lld','%d','%s','%s')",

            jobType, sid, pid, uid, (int) lang, postTime.c_str(), cid, cases, timeLimit.count(), timeLimit, memoryLimit,
            this->similarity_threshold, (int) result.judge_result, result.cpu_time.count(),
            result.memory.count(), result.similarity_percentage, escape_code, escape_compile_error_info);
    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_FATAL(jobType, sid, log_fp,
                  "insert into core_update_failed table failed again!,errno:%d, error:%s\n        sql:%s",
                  mysql_errno(conn), mysql_error(conn), sql);
    }
    return ret;
}
