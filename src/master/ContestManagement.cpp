/*
 * ContestManagement.cpp
 *
 *  Created on: 2018年11月9日
 *      Author: peter
 */

#include "ContestManagement.hpp"
#include "mysql_empty_res_exception.hpp"

#include <set>


void ContestManagement::update_problem_submit_and_accept_num(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::p_id_type p_id)
{
	std::set<ojv4::u_id_type> accepted_users;
	int submit_num = 0;
	{
		mysqlpp::Query query = mysql_conn.query(
				"select u_id, s_result from contest_solution%0 where p_id = %1 order by s_id"
		);
		query.parse();

		mysqlpp::StoreQueryResult res = query.store(ct_id, p_id);

		if (query.errnum() != 0) {
			throw MysqlEmptyResException(query.errnum(), query.error());
		}

		for (const auto & row : res) {
			ojv4::u_id_type u_id_in_this_row = row["u_id"];
			// 此题已通过的用户的集合中无此条 solution 对应的用户
			if (accepted_users.find(u_id_in_this_row) == accepted_users.end()) {
				ojv4::s_result_enum_type result = ojv4::s_result_enum_type(ojv4::s_result_in_db_type(row["s_result"]));
				switch(result) {
					case UnitedJudgeResult::ACCEPTED:
						accepted_users.insert(u_id_in_this_row);
						++submit_num;
						break;
					case UnitedJudgeResult::SYSTEM_ERROR: // ignore system error
						break;
					default:
						++submit_num;
						break;
				}
			}
		}
	}

	{
		mysqlpp::Query update = mysql_conn.query(
				"update contest_problem set ct_p_submit = %0, ct_p_accept = %1 where p_id = %2 and ct_id = %3"
		);
		update.parse();

		mysqlpp::SimpleResult update_res = update.execute(submit_num, accepted_users.size(), p_id, ct_id);
		if (!update_res) {
			throw MysqlEmptyResException(update.errnum(), update.error());
		}
	}
}


void ContestManagement::update_user_problem_status(mysqlpp::Connection & mysql_conn, ojv4::ct_id_type ct_id, ojv4::u_id_type u_id, ojv4::p_id_type p_id)
{
	constexpr ojv4::s_result_in_db_type ACCEPTED = ojv4::s_result_in_db_type(ojv4::s_result_enum_type::ACCEPTED);
	constexpr ojv4::s_result_in_db_type SYSTEM_ERROR = ojv4::s_result_in_db_type(ojv4::s_result_enum_type::SYSTEM_ERROR);

	mysqlpp::Transaction trans(mysql_conn,
						mysqlpp::Transaction::serializable,
						mysqlpp::Transaction::session);

	mysqlpp::Query drop = mysql_conn.query(
			"drop table contest_user_problem_solution_%0_%1_%2 if exist contest_user_problem_solution_%0_%1_%2"
	);
	drop.parse();

	if(!drop.execute(ct_id, u_id, p_id)) {
		MysqlEmptyResException e(mysql_conn.errnum(), mysql_conn.error());
		throw e;
	}

	mysqlpp::Query create_table = mysql_conn.query(
			"create table contest_user_problem_solution_%0_%1_%2 type='heap' "
			"select s_id, s_posttime, s_result from contest_solution%0 "
			"where u_id = %1 and p_id = %2 and s_result <> %4"
	);
	create_table.parse();

	if(!create_table.execute(ct_id, u_id, p_id, ACCEPTED, SYSTEM_ERROR)) {
		MysqlEmptyResException e(mysql_conn.errnum(), mysql_conn.error());
		throw e;
	}

	mysqlpp::Query insert = mysql_conn.query(
			"insert into contest_user_problem%0 (u_id, p_id, is_ac, ac_time, error_count) "
			"values (%1, %2, "
			"((select count(s_id) from contest_user_problem_solution_%0_%1_%2 where s_result = %3) = 0), " // is_ac
			"(select min(s_posttime) from contest_user_problem_solution_%0_%1_%2 where s_result = %3), " // ac_time
			"(select ) "
			")"
	);
	insert.parse();

	if (!insert.execute(ct_id, u_id, p_id, ACCEPTED, SYSTEM_ERROR)) {
		mysqlpp::Query update = mysql_conn.query(
				"update contest_user_problem%0  set "
				"is_ac = ((select count(s_id) from contest_user_problem_solution_%0_%1_%2 where s_result = %3) = 0), " // is_ac
				"ac_time = (select min(s_posttime) from contest_user_problem_solution_%0_%1_%2 where s_result = %3), " // ac_time
				"error_count = () "
				"where u_id = %1 and p_id = %2"
		);
		update.parse();
		if(!update.execute(ct_id, u_id, p_id, ACCEPTED, SYSTEM_ERROR)) {
			MysqlEmptyResException e(mysql_conn.errnum(), mysql_conn.error());
			throw e;
		}
	}

	if(!drop.execute(ct_id, u_id, p_id)) {
		MysqlEmptyResException e(mysql_conn.errnum(), mysql_conn.error());
		throw e;
	}

	trans.commit();
}


