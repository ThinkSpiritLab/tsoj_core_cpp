/*
 * rejudge.cpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#include "UpdatorJob.hpp"
#include "logger.hpp"

#include <kerbal/mysql/mysql_command.hpp>
using namespace kerbal::mysql;

#include <cstring>

extern std::fstream log_fp;

enum user_problem_status {
    // ACCEPTED = 0, defined in compare_result
    ATTEMPTED = 1,
};

#define SQL_BUFFER  1024*1024
#define FILE_SIZE   1024*1024

static char sql[SQL_BUFFER];


int UpdatorJob::rejudge(const Result & result)
{
	//获取原始评测结果
	std::string solution_table;
	if (jobType == 0) {
		solution_table = "solution";
	} else {
		solution_table = "contest_solution%d"_fmt(jobType).str();
	}
	mysqlpp::Query query = mysqlConn.query("select s_result, s_time, s_mem, s_similarity_percentage from %0 where s_id = %1");
	query.parse();

	mysqlpp::StoreQueryResult res;
	try {
		res = query.store(solution_table, sid);
	} catch (const mysqlpp::BadQuery & e) {
		LOG_FATAL(jobType, sid, log_fp, "Select original result from solution failed! Exception info: ", e.what());
		return query.errnum();
	}

	if (res.empty()) {
		LOG_FATAL(jobType, sid, log_fp, "Fetch row from resultset failed!");
		return -1;
	}

	const mysqlpp::Row & row = res[0];

	UnitedJudgeResult orig_result = (UnitedJudgeResult) std::stoi(row[0]);
	//更新记录
	if (rejudge_solution(result)) {
		return -1;
	}
	if (jobType == 0) {
		if (cid) {
			if (rejudge_course_user(result, orig_result)) {
				return -1;
			}
		}
		if (rejudge_user_and_problem(result, orig_result)) {
			return -1;
		}
		if (rejudge_user_problem()) {
			return -1;
		}
	} else {
		if (rejudge_contest_user_problem()) {
			return -1;
		}
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
		try {
			del_ce_info_templ.store(compile_info_table, sid);
		} catch (const mysqlpp::BadQuery & e) {
			LOG_WARNING(jobType, sid, log_fp, "Delete compile info failed! errno: ", e.what());
			return query.errnum();
		}
	}
	//将原始评测结果插入rejudge_solution表内
	mysqlpp::Query insert_rejudge_solution_templ = mysqlConn.query(
			"insert into rejudge_solution(ct_id, s_id, orig_result, orig_time, orig_mem, orig_similarity_percentage) "
			"values ('%d', '%d', '%d', '%d', '%lld', '%d')");
	insert_rejudge_solution_templ.parse();
	try {
		insert_rejudge_solution_templ.store(jobType, sid, orig_result, std::stoi(row[1]), std::stoll(row[2]), std::stoi(row[3]));
	} catch (const MysqlException & e) {
		LOG_WARNING(jobType, sid, log_fp, "Insert into rejudge_solution failed! errno: ", e.what());
		return conn.get_errno();
	}

	if (jobType == 0) {
		//将rejudge提示信息插入message表内，并不允许回复
		static MysqlCommand templ("insert into message(u_id, u_receiver, m_content, m_status) "
				"values ('1', '%d', '您于%s提交的问题%d的代码已经被重新评测，请查询。', '3')");
		try {
			templ.execute(conn, uid, postTime.c_str(), pid);
		} catch (const MysqlException & e) {
			LOG_WARNING(jobType, sid, log_fp, "Insert into message failed! errno: ", e.what());
			return conn.get_errno();
		}
	}
	return 0;
}

int UpdatorJob::rejudge_solution(const MysqlContext & conn, const Result & result)
{
	std::string solution_table;
	if (!jobType) {
		solution_table = "solution";
	} else {
		solution_table = (boost::format("contest_solution%d") % jobType).str();
	}
	static MysqlCommand cmd("update %s"
			"set s_result = '%d', s_time = '%d', s_mem = '%lld', s_similarity_percentage = '%d' "
			"where s_id = '%d'");
	MysqlRes query_res;
	try {
		query_res = cmd.execute(conn, solution_table, (int) result.judge_result, result.cpu_time.count(), result.memory.count(), result.similarity_percentage, sid);
	} catch (const MysqlException & e) {
		LOG_WARNING(jobType, sid, log_fp, "Rejudge solution failed!, Exception info: ", e.what());
		return e.get_errno();
	}
	return 0;
}


int UpdatorJob::rejudge_user_and_problem(MYSQL *conn, const Result &_result, UnitedJudgeResult orig_result)
{
	char cid_param[10];
	memset(cid_param, 0, sizeof(cid_param));
	if (cid) {
		sprintf(cid_param, "= '%d'", cid);
	} else {
		sprintf(cid_param, "is null");
	}

	MYSQL_RES *res;
	MYSQL_ROW row;
	memset(sql, 0, sizeof(sql));
	//查询在本次rejudge的提交前后课程中当前题目的通过情况
	sprintf(sql, "select count(case when s_posttime < '%s' and s_result = '0' then 1 end), "
			"count(case when s_posttime > '%s' and s_result = '0' then 1 end), "
			"count(case when s_posttime > '%s' then 1 end) "
			"from solution where c_id %s and u_id = '%d' and p_id = '%d'", postTime.c_str(), postTime.c_str(), postTime.c_str(), cid_param, uid, pid);
	int ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return mysql_errno(conn);
	}
	row = mysql_fetch_row(res);
	if (row == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
		mysql_free_result(res);
		return -1;
	}
	int acbefore = atoi(row[0]), acafter = atoi(row[1]), submitafter = atoi(row[2]);
	mysql_free_result(res);
	memset(sql, 0, sizeof(sql));
	//如果之前AC过则无需更新；如果之前没AC过且评测结果发生改变则需要更新
	if (acbefore == 0
			&& ((orig_result == UnitedJudgeResult::ACCEPTED && _result.judge_result != UnitedJudgeResult::ACCEPTED)
					|| (orig_result != UnitedJudgeResult::ACCEPTED && _result.judge_result == UnitedJudgeResult::ACCEPTED))) {
		if (acafter > 0) { //如果在rejudge的solution后有原来AC的solution
			//查询rejudge的solution与最近的下一次AC间有几次提交
			sprintf(sql, "select count(*) from solution "
					"where s_posttime > '%s' and s_posttime < all ("
					"select s_posttime from solution "
					"where c_id %s and u_id = '%d' and p_id = '%d' and s_posttime > '%s' and s_result = '0') "
					"and c_id %s and u_id = '%d' and p_id = '%d'", postTime.c_str(), cid_param, uid, pid, postTime.c_str(), cid_param, uid, pid);
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
			res = mysql_store_result(conn);
			if (res == NULL) {
				LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return mysql_errno(conn);
			}
			row = mysql_fetch_row(res);
			if (row == NULL) {
				LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
				mysql_free_result(res);
				return -1;
			}
			memset(sql, 0, sizeof(sql));
			//更新user表
			if (_result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update user set u_submit = u_submit - %d - 1 where u_id = '%d'", atoi(row[0]), uid);
			} else {
				sprintf(sql, "update user set u_submit = u_submit + %d + 1 where u_id = '%d'", atoi(row[0]), uid);
			}
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
			memset(sql, 0, sizeof(sql));
			//更新problem表
			if (_result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update problem set p_submit = p_submit - %d - 1 where p_id = '%d'", atoi(row[0]), pid);
			} else {
				sprintf(sql, "update problem set p_submit = p_submit + %d + 1 where p_id = '%d'", atoi(row[0]), pid);
			}
			mysql_free_result(res);
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
		} else { //如果在rejudge的solution后有原来AC的solution
			//更新user表
			if (_result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update user set u_submit = u_submit - %d, u_accept = u_accept + 1 where u_id = '%d'", submitafter, uid);
			} else {
				sprintf(sql, "update user set u_submit = u_submit + %d, u_accept = u_accept - 1 where u_id = '%d'", submitafter, uid);
			}
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
			memset(sql, 0, sizeof(sql));
			//更新problem表
			if (_result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update problem set p_submit = p_submit - %d, p_accept = p_accept + 1 where p_id = '%d'", submitafter, pid);
			} else {
				sprintf(sql, "update problem set p_submit = p_submit + %d, p_accept = p_accept - 1 where p_id = '%d'", submitafter, pid);
			}
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
		}
	}
	return 0;
}

int UpdatorJob::rejudge_course_user(MYSQL *conn, const Result & result, UnitedJudgeResult orig_result)
{
	MYSQL_RES *res;
	MYSQL_ROW row;

	//查询在本次rejudge的提交前后课程中当前题目的通过情况
	memset(sql, 0, sizeof(sql));
	sprintf(sql,
			"select count(case when s_posttime < '%s' and s_result = '0' then 1 end), "
			"count(case when s_posttime > '%s' and s_result = '0' then 1 end), "
			"count(case when s_posttime > '%s' then 1 end) "
			"from solution where c_id = '%d' and u_id = '%d' and p_id = '%d'",
			postTime.c_str(), postTime.c_str(), postTime.c_str(), cid, uid, pid);
	int ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return mysql_errno(conn);
	}
	row = mysql_fetch_row(res);
	if (row == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
		mysql_free_result(res);
		return -1;
	}
	int acbefore = atoi(row[0]), acafter = atoi(row[1]), submitafter = atoi(row[2]);
	mysql_free_result(res);

	memset(sql, 0, sizeof(sql));
	//如果之前AC过则无需更新；如果之前没AC过且评测结果发生改变则需要更新
	if (acbefore == 0
			&& ((orig_result == UnitedJudgeResult::ACCEPTED && result.judge_result != UnitedJudgeResult::ACCEPTED)
					|| (orig_result != UnitedJudgeResult::ACCEPTED && result.judge_result == UnitedJudgeResult::ACCEPTED))) {
		//如果在rejudge的solution后有原来AC的solution
		if (acafter > 0) {
			//查询rejudge的solution与最近的下一次AC间有几次提交
			sprintf(sql, "select count(*) from solution "
					"where s_posttime > '%s' and s_posttime < all ("
					"select s_posttime from solution where c_id = '%d' and u_id = '%d' and p_id = '%d' and s_posttime > '%s' and s_result = '0') "
					"and c_id = '%d' and u_id = '%d' and p_id = '%d'", postTime.c_str(), cid, uid, pid, postTime.c_str(), cid, uid, pid);
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
			res = mysql_store_result(conn);
			if (res == NULL) {
				LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return mysql_errno(conn);
			}
			row = mysql_fetch_row(res);
			if (row == NULL) {
				LOG_FATAL(jobType, sid, log_fp, "REJUDGE: fetch row from resultset failed!");
				mysql_free_result(res);
				return -1;
			}
			memset(sql, 0, sizeof(sql));
			//更新course_user
			if (result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update course_user set c_submit = c_submit - %d - 1 where c_id = '%d' and u_id = '%d'", atoi(row[0]), cid, uid);
			} else {
				sprintf(sql, "update course_user set c_submit = c_submit + %d + 1 where c_id = '%d' and u_id = '%d'", atoi(row[0]), cid, uid);
			}
			mysql_free_result(res);
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update course_user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
		} else { //如果在rejudge的solution后没有原来AC的solution
			if (result.judge_result == UnitedJudgeResult::ACCEPTED) {
				sprintf(sql, "update course_user set c_submit = c_submit - %d, c_accept = c_accept + 1 where c_id = '%d' and u_id = '%d'", submitafter, cid, uid);
			} else {
				sprintf(sql, "update course_user set c_submit = c_submit + %d, c_accept = c_accept - 1 where c_id = '%d' and u_id = '%d'", submitafter, cid, uid);
			}
			ret = mysql_real_query(conn, sql, strlen(sql));
			if (ret) {
				LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update course_user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
				return ret;
			}
		}
	}
	return 0;
}

int UpdatorJob::rejudge_user_problem(MYSQL *conn)
{
	MYSQL_RES *res;
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
	sprintf(sql, "select count(case when s_result = '0' then 1 end), count(*) from solution where u_id = '%d' and p_id = '%d' and c_id %s", uid, pid, cid_param);
	int ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
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
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select status from user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return mysql_errno(conn);
	}
	row = mysql_fetch_row(res);
	memset(sql, 0, sizeof(sql));
	if (row == NULL) {
		if (stat == 0 || stat == 1) {
			if (cid) {
				sprintf(sql, "insert into user_problem (u_id, p_id, c_id, status) values ('%d', '%d', '%d', '%d')", uid, pid, cid, stat);
			} else {
				sprintf(sql, "insert into user_problem (u_id, p_id, status) values ('%d', '%d', '%d')", uid, pid, stat);
			}
		}
	} else {
		if (stat == 0 || stat == 1) {
			sprintf(sql, "update user_problem set status = '%d' where u_id = '%d' and p_id = '%d' and c_id %s", stat, uid, pid, cid_param);
		} else {
			sprintf(sql, "delete from user_problem where u_id = '%d' and p_id = '%d' and c_id %s", uid, pid, cid_param);
		}
	}
	mysql_free_result(res);
	ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
	}
	return ret;
}

int UpdatorJob::rejudge_contest_user_problem(MYSQL *conn)
{
	MYSQL_RES *res;
	MYSQL_ROW row;

	memset(sql, 0, sizeof(sql));
	//更新contest_problem表中first_ac_user
	sprintf(sql, "select u_id from contest_solution%d where p_id = '%d' and s_result = '0' order by s_posttime asc limit 1", jobType, pid);
	int ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select first_ac_user failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%d\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return mysql_errno(conn);
	}
	row = mysql_fetch_row(res);
	memset(sql, 0, sizeof(sql));
	int first_ac_user = 0;
	if (row == NULL) {
		sprintf(sql, "update contest_problem set first_ac_user = NULL where ct_id = '%d' and p_id = '%d'", jobType, pid);
	} else {
		first_ac_user = atoi(row[0]);
		sprintf(sql, "update contest_problem set first_ac_user = '%d' where ct_id = '%d' and p_id = '%d'", first_ac_user, jobType, pid);
	}
	mysql_free_result(res);
	ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update first_ac_user in contest_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	//更新contest_user_problem表中的is_first_ac
	//先将所有本题记录的is_first_ac设为0
	memset(sql, 0, sizeof(sql));
	sprintf(sql, "update contest_user_problem%d set is_first_ac = '0' where p_id = '%d'", jobType, pid);
	ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update contest_user_problem set is_first_ac to zero failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}

	memset(sql, 0, sizeof(sql));
	if (first_ac_user) {
		sprintf(sql, "update contest_user_problem%d set is_first_ac = '1' where u_id = '%d' and p_id = '%d'", jobType, first_ac_user, pid);
		ret = mysql_real_query(conn, sql, strlen(sql));
		if (ret) {
			LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update contest_user_problem set is_first_ac failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
			return ret;
		}
	}

	memset(sql, 0, sizeof(sql));
	//更新contest_user_problem表，查询本题通过数和提交数
	sprintf(sql, "select count(case when s_result = '0' then 1 end), count(*) from contest_solution%d "
			"where u_id = '%d' and p_id = '%d'", jobType, uid, pid);
	ret = mysql_real_query(conn, sql, strlen(sql));
	if (ret) {
		LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from contest_solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		return ret;
	}
	res = mysql_store_result(conn);
	if (res == NULL) {
		LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
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
			LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select count info from contest_solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
			return ret;
		}
		res = mysql_store_result(conn);
		if (res == NULL) {
			LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
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
			LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select s_posttime from contest_solution failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
			return ret;
		}
		res = mysql_store_result(conn);
		if (res == NULL) {
			LOG_FATAL(jobType, sid, log_fp, "REJUDGE: store result failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
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
			LOG_WARNING(jobType, sid, log_fp, "REJUDGE: select update contest_user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		}
	} else { //如果没有通过过
		sprintf(sql, "update contest_user_problem%d "
				"set is_ac = '0', is_bal = NULL, ac_time = NULL, error_count = '%d', is_first_ac = '0' "
				"where u_id = '%d' and p_id = '%d'", jobType, submitnum, uid, pid);
		ret = mysql_real_query(conn, sql, strlen(sql));
		if (ret) {
			LOG_WARNING(jobType, sid, log_fp, "REJUDGE: update contest_user_problem failed!, errno:%d, error:%s\nsql:%s", mysql_errno(conn), mysql_error(conn), sql);
		}
	}
	return ret;
}
