/*
 * rejudge.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#include "logger.hpp"
#include "boost_format_suffix.hpp"


#include <cstring>
#include "../master_old/UpdatorJob.hpp"

extern std::fstream log_fp;

enum user_problem_status
{
    // ACCEPTED = 0, defined in compare_result
            ATTEMPTED = 1,
};


void UpdatorJob::rejudge(const Result & result)
{
    //获取原始评测结果
    std::string solution_table;
    if (jobType == 0) {
        solution_table = "solution";
    } else {
        solution_table = "contest_solution%d"_fmt(jobType).str();
    }
    mysqlpp::Query query = mysqlConn.query(
            "select s_result, s_time, s_mem, s_similarity_percentage "
            "from %0 where s_id = %1"
    );
    query.parse();

    mysqlpp::StoreQueryResult res;
    try {
        res = query.store(solution_table, sid);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error information: ", e.what());
        throw;
    }

    if (res.empty()) {
        if (query.errnum() == 0) {
            LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error information: ", "empty set");
            throw std::logic_error("empty set");
        } else {
            LOG_FATAL(jobType, sid, log_fp, "Fetch row from result set failed! Error code: ", query.errnum(),
                      ", Error information: ", query.error());
            throw std::logic_error(query.error());
        }
    }

    const mysqlpp::Row & row = res[0];

    UnitedJudgeResult orig_result = (UnitedJudgeResult) std::stoi(row[0]);
    //更新记录
    rejudge_solution(result);
    if (jobType == 0) {
        if (cid) {
            rejudge_course_user(result, orig_result);
        }
        rejudge_user_and_problem(result, orig_result);
        rejudge_user_problem();
    } else {
        rejudge_contest_user_problem();
    }

    //如果原始评测结果为COMPILE_ERROR则从数据库删除编译错误信息
    if (orig_result == UnitedJudgeResult::COMPILE_ERROR) {
        std::string compile_info_table;
        if (jobType == 0) {
            compile_info_table = "compile_info";
        } else {
            compile_info_table = "contest_compile_info%d"_fmt(jobType).str();
        }
        mysqlpp::Query del_ce_info_templ = mysqlConn.query("delete from %0 where s_id = %1");
        del_ce_info_templ.parse();
        mysqlpp::SimpleResult res;
        try {
            del_ce_info_templ.execute(compile_info_table, sid);
        } catch (const mysqlpp::BadParamCount & e) {
            LOG_FATAL(jobType, sid, log_fp, "Delete compile info failed! Error information: ", e.what());
            throw;
        } catch (const mysqlpp::BadQuery & e) {
        	LOG_FATAL(jobType, sid, log_fp, "Delete compile info failed! Error information: ", e.what());
            throw;
        }
        if (!res) {
            LOG_FATAL(jobType, sid, log_fp, "Delete compile info failed! ",
                      "Error code: ", del_ce_info_templ.errnum(),
                      ", Error information: ", del_ce_info_templ.error());
            throw std::logic_error(del_ce_info_templ.error());
        }
    }

    //将原始评测结果插入rejudge_solution表内
    {
        mysqlpp::Query insert_rejudge_solution_templ = mysqlConn.query(
                "insert into rejudge_solution(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage) "
                "values (%0, %1, %2, %3q, %4, %5)"
        );
        mysqlpp::SimpleResult res;
        insert_rejudge_solution_templ.parse();
        try {
            res = insert_rejudge_solution_templ.execute(jobType, sid, orig_result, std::stoi(row[1]),
                                                        std::stoll(row[2]),
                                                        std::stoi(row[3]));
        } catch (const mysqlpp::BadParamCount & e) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into rejudge_solution failed! Error information: ", e.what());
            throw;
        } catch (const mysqlpp::BadQuery & e) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into rejudge_solution failed! Error information: ", e.what());
            throw;
        }
        if (!res) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into rejudge_solution failed! ",
                      "Error code: ", insert_rejudge_solution_templ.errnum(),
                      ", Error information: ", insert_rejudge_solution_templ.error());
            throw std::logic_error(insert_rejudge_solution_templ.error());
        }
    }

    if (jobType == 0) {
        //将rejudge提示信息插入message表内，并不允许回复
        mysqlpp::Query insert_into_message_templ = mysqlConn.query(
                "insert into message(u_id, u_receiver, m_content, m_status) "
                "values (1, %0, '您于%1q提交的问题%2的代码已经被重新评测，请查询。', 3)"
        );
        insert_into_message_templ.parse();
        mysqlpp::SimpleResult res;
        try {
            res = insert_into_message_templ.execute(uid, postTime, pid);
        } catch (const mysqlpp::BadParamCount & e) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into message failed! Error information: ", e.what());
            throw;
        } catch (const mysqlpp::BadQuery & e) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into message failed! Error information: ", e.what());
            throw;
        }
        if (!res) {
            LOG_FATAL(jobType, sid, log_fp, "Insert into message failed! ",
                      "Error code: ", insert_into_message_templ.errnum(),
                      ", Error information: ", insert_into_message_templ.error());
            throw std::logic_error(insert_into_message_templ.error());
        }
    }
}

void UpdatorJob::rejudge_solution(const Result & result)
{
    std::string solution_table;
    if (!jobType) {
        solution_table = "solution";
    } else {
        solution_table = "contest_solution%d"_fmt(jobType).str();
    }
    mysqlpp::Query update = mysqlConn.query(
            "update %0"
            "set s_result = %1, s_time = %2, s_mem = %3, s_similarity_percentage = %4 "
            "where s_id = %5"
    );
    update.parse();
    mysqlpp::SimpleResult res;
    try {
        res = update.execute(solution_table, (int) result.judge_result, result.cpu_time.count(),
                             result.memory.count(), result.similarity_percentage, sid);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution after rejudge failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution after rejudge failed! Error information: ", e.what());
        throw;
    }
    if (!res) {
        LOG_FATAL(jobType, sid, log_fp, "Update solution after rejudge failed! ",
                  "Error code: ", update.errnum(),
                  ", Error information: ", update.error());
        throw std::logic_error(update.error());
    }
}


void UpdatorJob::rejudge_user_and_problem(const Result & result, UnitedJudgeResult orig_result)
{
    std::string cid_param;
    if (cid) {
        cid_param = "= %d"_fmt(cid).str();
    } else {
        cid_param = "is null";
    }

    //查询在本次rejudge的提交前后课程中当前题目的通过情况
    mysqlpp::Query query = mysqlConn.query(
            "select count(case when s_posttime < %0q and s_result = 0 then 1 end), "
            "count(case when s_posttime > %1q and s_result = 0 then 1 end), "
            "count(case when s_posttime > %2q then 1 end) "
            "from solution where c_id %3q and u_id = %4 and p_id = %5"
    );
    query.parse();
    mysqlpp::StoreQueryResult res;
    try {
        res = query.store(postTime.c_str(), postTime.c_str(), postTime.c_str(), cid_param, uid, pid);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", e.what());
        throw;
    }

    if (res.empty()) {
        if (query.errnum() == 0) {
            LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", "empty set");
            throw std::logic_error("empty set");
        } else {
            LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! ",
                      "Error code: ", query.errnum(),
                      ", Error information: ", query.error());
            throw std::logic_error(query.error());
        }
    }

    const mysqlpp::Row & row = res[0];
    int acbefore = atoi(row[0]), acafter = atoi(row[1]), submitafter = atoi(row[2]);

    //如果之前AC过则无需更新；如果之前没AC过且评测结果发生改变则需要更新
    if (acbefore == 0
        && ((orig_result == UnitedJudgeResult::ACCEPTED && result.judge_result != UnitedJudgeResult::ACCEPTED)
            || (orig_result != UnitedJudgeResult::ACCEPTED && result.judge_result == UnitedJudgeResult::ACCEPTED))) {
        if (acafter > 0) { //如果在rejudge的solution后有原来AC的solution
            //查询rejudge的solution与最近的下一次AC间有几次提交
            {
                mysqlpp::Query query = mysqlConn.query(
                        "select count(*) from solution "
                        "where s_posttime > %0q and s_posttime < all ("
                        "    select s_posttime from solution "
                        "    where c_id %1 and u_id = %2 and p_id = %3 and s_posttime > %4q and s_result = 0 "
                        ") "
                        "and c_id %5 and u_id = %6 and p_id = %7"
                );
                query.parse();
                mysqlpp::StoreQueryResult res;
                try {
                    query.store(postTime, cid_param, uid, pid,
                                postTime, cid_param, uid, pid);
                } catch (const mysqlpp::BadParamCount & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ",
                              e.what());
                    throw;
                } catch (const mysqlpp::BadQuery & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ",
                              e.what());
                    throw;
                }

                if (res.empty()) {
                    if (query.errnum() == 0) {
                        LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ",
                                  "empty set");
                        throw std::logic_error("empty set");
                    } else {
                        LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! ",
                                  "Error code: ", query.errnum(),
                                  ", Error information: ", query.error());
                        throw std::logic_error(query.error());
                    }
                }
            }

            //更新user表
            {
                mysqlpp::Query query = mysqlConn.query(
                        (result.judge_result == UnitedJudgeResult::ACCEPTED)
                        ?
                        "update user set u_submit = u_submit - %d - 1 where u_id = %d"
                        :
                        "update user set u_submit = u_submit + %d + 1 where u_id = %d"
                );
                query.parse();
                mysqlpp::SimpleResult res;
                try {
                    res = query.execute(std::stoi(row[0]), uid);
                } catch (const mysqlpp::BadParamCount & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
                    throw;
                } catch (const mysqlpp::BadQuery & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
                    throw;
                }

                if (!res) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! ",
                              "Error code: ", query.errnum(),
                              ", Error information: ", query.error());
                    throw std::logic_error(query.error());
                }
            }

            //更新problem表
            {
                mysqlpp::Query query = mysqlConn.query(
                        (result.judge_result == UnitedJudgeResult::ACCEPTED)
                        ?
                        "update problem set p_submit = p_submit - %d - 1 where p_id = %d"
                        :
                        "update problem set p_submit = p_submit + %d + 1 where p_id = %d"
                );
                query.parse();
                mysqlpp::SimpleResult res;
                try {
                    res = query.execute(std::stoi(row[0]), pid);
                } catch (const mysqlpp::BadParamCount & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
                    throw;
                } catch (const mysqlpp::BadQuery & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
                    throw;
                }

                if (!res) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! ",
                              "Error code: ", query.errnum(),
                              ", Error information: ", query.error());
                    throw std::logic_error(query.error());
                }
            }
        } else { //如果在rejudge的solution后有原来AC的solution
            //更新user表
            {
                mysqlpp::Query update = mysqlConn.query(
                        (result.judge_result == UnitedJudgeResult::ACCEPTED)
                        ?
                        "update user set u_submit = u_submit - %0, u_accept = u_accept + 1 where u_id = %1"
                        :
                        "update user set u_submit = u_submit + %0, u_accept = u_accept - 1 where u_id = %1"
                );
                update.parse();
                mysqlpp::SimpleResult res;
                try {
                    res = update.execute(submitafter, this->uid);
                } catch (const mysqlpp::BadParamCount & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
                    throw;
                } catch (const mysqlpp::BadQuery & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! Error information: ", e.what());
                    throw;
                }
                if (!res) {
                    LOG_FATAL(jobType, sid, log_fp, "Update user failed! ",
                              "Error code: ", query.errnum(),
                              ", Error information: ", query.error());
                    throw std::logic_error(query.error());
                }
            }

            //更新problem表
            {
                mysqlpp::Query update = mysqlConn.query(
                        (result.judge_result == UnitedJudgeResult::ACCEPTED)
                        ?
                        "update problem set p_submit = p_submit - %0, p_accept = p_accept + 1 where p_id = %1"
                        :
                        "update problem set p_submit = p_submit + %0, p_accept = p_accept - 1 where p_id = %1"
                );
                update.parse();
                mysqlpp::SimpleResult res;
                try {
                    res = update.execute(submitafter, pid);
                } catch (const mysqlpp::BadParamCount & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
                    throw;
                } catch (const mysqlpp::BadQuery & e) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! Error information: ", e.what());
                    throw;
                }
                if (!res) {
                    LOG_FATAL(jobType, sid, log_fp, "Update problem failed! ",
                              "Error code: ", query.errnum(),
                              ", Error information: ", query.error());
                    throw std::logic_error(query.error());
                }
            }
        }
    }
}

void UpdatorJob::rejudge_course_user(const Result & result, UnitedJudgeResult orig_result)
{
    //查询在本次rejudge的提交前后课程中当前题目的通过情况
    mysqlpp::Query query_solution = mysqlConn.query(
            "select count(case when s_posttime < %0q and s_result = 0 then 1 end), "
            "count(case when s_posttime > %1q and s_result = 0 then 1 end), "
            "count(case when s_posttime > %2q then 1 end) "
            "from solution where c_id = %3 and u_id = %4 and p_id = %5"
    );
    query_solution.parse();
    mysqlpp::StoreQueryResult res;
    try {
        res = query_solution.store(postTime, postTime, postTime, cid, uid, pid);
    } catch (const mysqlpp::BadParamCount & e) {
        LOG_FATAL(jobType, sid, log_fp,
                  "Select count info from solution failed! Error information: ", e.what());
        throw;
    } catch (const mysqlpp::BadQuery & e) {
        LOG_FATAL(jobType, sid, log_fp,
                  "Select count info from solution failed! Error information: ", e.what());
        throw;
    }
    if (res.empty()) {
        if (query_solution.errnum() == 0) {
            LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", "empty set");
            throw std::logic_error("empty set");
        } else {
            LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! ",
                      "Error code: ", query_solution.errnum(),
                      ", Error information: ", query_solution.error());
            throw std::logic_error(query_solution.error());
        }
    }
    int acbefore = std::stoi(res[0][0]), acafter = std::stoi(res[0][1]), submitafter = std::stoi(res[0][2]);

    //如果之前AC过则无需更新；如果之前没AC过且评测结果发生改变则需要更新
    if (acbefore == 0
        && ((orig_result == UnitedJudgeResult::ACCEPTED && result.judge_result != UnitedJudgeResult::ACCEPTED)
            || (orig_result != UnitedJudgeResult::ACCEPTED && result.judge_result == UnitedJudgeResult::ACCEPTED))) {
        //如果在rejudge的solution后有原来AC的solution
        if (acafter > 0) {
            //查询rejudge的solution与最近的下一次AC间有几次提交
            mysqlpp::Query query = mysqlConn.query(
                    "select count(*) from solution "
                    "where s_posttime > %0q and s_posttime < all ("
                    "    select s_posttime from solution "
                    "    where c_id = %1 and u_id = %2 and p_id = %3 and s_posttime > %4q and s_result = 0"
                    ") and "
                    "c_id = %5 and u_id = %6 and p_id = %7"
            );
            query.parse();

            mysqlpp::StoreQueryResult res;
            try {
                res = query.store(postTime, cid, uid, pid,
                                  postTime, cid, uid, pid);
            } catch (const mysqlpp::BadParamCount & e) {
                LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", e.what());
                throw;
            } catch (const mysqlpp::BadQuery & e) {
                LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", e.what());
                throw;
            }
            if (res.empty()) {
                if (query_solution.errnum() == 0) {
                    LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! Error information: ", "empty set");
                    throw std::logic_error("empty set");
                } else {
                    LOG_FATAL(jobType, sid, log_fp, "Select count info from solution failed! ",
                              "Error code: ", query_solution.errnum(),
                              ", Error information: ", query_solution.error());
                    throw std::logic_error(query_solution.error());
                }

            }
            const mysqlpp::Row & row = res[0];

            //更新course_user
            {
				mysqlpp::Query update = mysqlConn.query(
						(result.judge_result == UnitedJudgeResult::ACCEPTED)
						?
						"update course_user set c_submit = c_submit - %0 - 1 where c_id = %1 and u_id = %2"
						:
						"update course_user set c_submit = c_submit + %0 + 1 where c_id = %1 and u_id = %2"
					);
				update.parse();
				mysqlpp::SimpleResult res;
				try {
					res = update.execute(std::stoi(row[0]), cid, uid);
				} catch (const mysqlpp::BadParamCount & e) {
					LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! Error information: ", e.what());
					throw;
				} catch (const mysqlpp::BadQuery & e) {
					LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! Error information: ", e.what());
					throw;
				}
				if (!res) {
					LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! ",
							  "Error code: ", update.errnum(),
							  ", Error information: ", update.error());
					throw std::logic_error(update.error());
				}
            }
        } else { //如果在rejudge的solution后没有原来AC的solution
			mysqlpp::Query update = mysqlConn.query(
					(result.judge_result == UnitedJudgeResult::ACCEPTED)
					?
					"update course_user set c_submit = c_submit - %0, c_accept = c_accept + 1 where c_id = %1 and u_id = %2"
					:
					"update course_user set c_submit = c_submit + %0, c_accept = c_accept - 1 where c_id = %1 and u_id = %2"
				);
			update.parse();
			mysqlpp::SimpleResult res;
			try {
				res = update.execute(submitafter, cid, uid);
			} catch (const mysqlpp::BadParamCount & e) {
				LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! Error information: ", e.what());
				throw;
			} catch (const mysqlpp::BadQuery & e) {
				LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! Error information: ", e.what());
				throw;
			}
			if (!res) {
				LOG_FATAL(jobType, sid, log_fp, "Update course_user failed! ",
						  "Error code: ", update.errnum(),
						  ", Error information: ", update.error());
				throw std::logic_error(update.error());
			}
        }
    }
}

int UpdatorJob::rejudge_user_problem(MYSQL * conn)
{
    MYSQL_RES * res;
    MYSQL_ROW row;

    char cid_param[10];
    memset(cid_param, 0, sizeof(cid_param));
    if (cid) {
        sprintf(cid_param, "= '%d'", cid);
    } else {
        sprintf(cid_param, "is NULL");
    }

    memset(sql, 0, sizeof(sql));
    //查询通过数和提交数
    sprintf(sql,
            "select count(case when s_result = '0' then 1 end), count(*) from solution where u_id = '%d' and p_id = '%d' and c_id %s",
            uid, pid, cid_param);
    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "REJUDGE: select count info from user_problem failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    if (row == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
        mysql_free_result(res);
        return -1;
    }

    //0:通过,1:提交过但未通过,2:未提交过
    int stat;
    if (atoi(row[0])) {
        stat = 0;
    } else if (atoi(row[1])) {
        stat = 1;
    } else {
        stat = 2;
    }
    mysql_free_result(res);

    memset(sql, 0, sizeof(sql));
    //查找原来的user_problem记录
    sprintf(sql, "select status from user_problem where u_id = '%d' and p_id = '%d' and c_id %s", uid, pid, cid_param);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "REJUDGE: select status from user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                    mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    memset(sql, 0, sizeof(sql));
    if (row == NULL) {
        if (stat == 0 || stat == 1) {
            if (cid) {
                sprintf(sql, "insert into user_problem (u_id, p_id, c_id, status) values ('%d', '%d', '%d', '%d')", uid,
                        pid, cid, stat);
            } else {
                sprintf(sql, "insert into user_problem (u_id, p_id, status) values ('%d', '%d', '%d')", uid, pid, stat);
            }
        }
    } else {
        if (stat == 0 || stat == 1) {
            sprintf(sql, "update user_problem set status = '%d' where u_id = '%d' and p_id = '%d' and c_id %s", stat,
                    uid, pid, cid_param);
        } else {
            sprintf(sql, "delete from user_problem where u_id = '%d' and p_id = '%d' and c_id %s", uid, pid, cid_param);
        }
    }
    mysql_free_result(res);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update user_problem failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
    }
    return ret;
}

int UpdatorJob::rejudge_contest_user_problem(MYSQL * conn)
{
    MYSQL_RES * res;
    MYSQL_ROW row;

    memset(sql, 0, sizeof(sql));
    //更新contest_problem表中first_ac_user
    sprintf(sql,
            "select u_id from contest_solution%d where p_id = '%d' and s_result = '0' order by s_posttime asc limit 1",
            jobType, pid);
    int ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select first_ac_user failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%d\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    memset(sql, 0, sizeof(sql));
    int first_ac_user = 0;
    if (row == NULL) {
        sprintf(sql, "update contest_problem set first_ac_user = NULL where ct_id = '%d' and p_id = '%d'", jobType,
                pid);
    } else {
        first_ac_user = atoi(row[0]);
        sprintf(sql, "update contest_problem set first_ac_user = '%d' where ct_id = '%d' and p_id = '%d'",
                first_ac_user, jobType, pid);
    }
    mysql_free_result(res);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "REJUDGE: update first_ac_user in contest_problem failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    //更新contest_user_problem表中的is_first_ac
    //先将所有本题记录的is_first_ac设为0
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "update contest_user_problem%d set is_first_ac = '0' where p_id = '%d'", jobType, pid);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "REJUDGE: update contest_user_problem set is_first_ac to zero failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }

    memset(sql, 0, sizeof(sql));
    if (first_ac_user) {
        sprintf(sql, "update contest_user_problem%d set is_first_ac = '1' where u_id = '%d' and p_id = '%d'", jobType,
                first_ac_user, pid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp,
                        "REJUDGE: update contest_user_problem set is_first_ac failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return ret;
        }
    }

    memset(sql, 0, sizeof(sql));
    //更新contest_user_problem表，查询本题通过数和提交数
    sprintf(sql, "select count(case when s_result = '0' then 1 end), count(*) from contest_solution%d "
                 "where u_id = '%d' and p_id = '%d'", jobType, uid, pid);
    ret = mysql_real_query(conn, sql, strlen(sql));
    if (ret) {
        LOG_WARNING(jobType, sid, log_fp,
                    "REJUDGE: select count info from contest_solution failed!, errno:%d, error:%s\nsql:%s",
                    mysql_errno(conn), mysql_error(conn), sql);
        return ret;
    }
    res = mysql_store_result(conn);
    if (res == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                  mysql_error(conn), sql);
        return mysql_errno(conn);
    }
    row = mysql_fetch_row(res);
    if (row == NULL) {
        LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
        mysql_free_result(res);
        return -1;
    }
    int acnum = atoi(row[0]), submitnum = atoi(row[1]);
    mysql_free_result(res);

    memset(sql, 0, sizeof(sql));
    if (acnum) { //如果通过过
        //查询第一次通过前提交次数
        sprintf(sql,
                "select count(*) from contest_solution%d "
                "where s_posttime < all ("
                "select s_posttime from contest_solution%d where u_id = '%d' and p_id = '%d' and s_result = '0') "
                "and u_id = '%d' and p_id = '%d'", jobType, jobType, uid, pid, uid, pid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp,
                        "REJUDGE: select count info from contest_solution failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return ret;
        }
        res = mysql_store_result(conn);
        if (res == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s",
                      mysql_errno(conn), mysql_error(conn), sql);
            return mysql_errno(conn);
        }
        row = mysql_fetch_row(res);
        if (row == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
            mysql_free_result(res);
            return -1;
        }
        int error_count = atoi(row[0]);
        mysql_free_result(res);

        memset(sql, 0, sizeof(sql));
        //查询第一次AC时间
        sprintf(sql, "select s_posttime from contest_solution%d "
                     "where u_id = '%d' and p_id = '%d' and s_result = '0' "
                     "order by s_posttime asc limit 1", jobType, uid, pid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp,
                        "REJUDGE: select s_posttime from contest_solution failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
            return ret;
        }
        res = mysql_store_result(conn);
        if (res == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s",
                      mysql_errno(conn), mysql_error(conn), sql);
            return mysql_errno(conn);
        }
        row = mysql_fetch_row(res);
        if (row == NULL) {
            LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
            mysql_free_result(res);
            return -1;
        }
        int is_first_ac = (first_ac_user == uid) ? 1 : 0;
        memset(sql, 0, sizeof(sql));
        //更新contest_user_problem表
        sprintf(sql, "update contest_user_problem%d "
                     "set is_ac = '1', ac_time = '%s', error_count = '%d', is_first_ac = '%d' "
                     "where u_id = '%d' and p_id = '%d'", jobType, row[0], error_count, is_first_ac, uid, pid);
        mysql_free_result(res);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp,
                        "REJUDGE: select update contest_user_problem failed!, errno:%d, error:%s\nsql:%s",
                        mysql_errno(conn), mysql_error(conn), sql);
        }
    } else { //如果没有通过过
        sprintf(sql, "update contest_user_problem%d "
                     "set is_ac = '0', is_bal = NULL, ac_time = NULL, error_count = '%d', is_first_ac = '0' "
                     "where u_id = '%d' and p_id = '%d'", jobType, submitnum, uid, pid);
        ret = mysql_real_query(conn, sql, strlen(sql));
        if (ret) {
            LOG_WARNING(jobType, sid, log_fp,
                        "REJUDGE: update contest_user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn),
                        mysql_error(conn), sql);
        }
    }
    return ret;
}
