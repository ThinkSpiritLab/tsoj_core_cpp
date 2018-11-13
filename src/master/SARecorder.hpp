/*
 * ProblemSARecorder.hpp
 *
 *  Created on: 2018年11月11日
 *      Author: peter
 */

#ifndef SRC_MASTER_SARECORDER_HPP_
#define SRC_MASTER_SARECORDER_HPP_

#include <set>

#include "ojv4_db_type.hpp"

class ProblemSARecorder
{
		int __submit_num;
		std::set<ojv4::u_id_type> accepted_users;

	public:
		ProblemSARecorder() :
			__submit_num(0), accepted_users()
		{
		}

		int submit_num() const
		{
			return __submit_num;
		}

		int accept_num() const
		{
			return this->accepted_users.size();
		}

		void add_solution(ojv4::u_id_type u_id, ojv4::s_result_enum_type s_result)
		{
			// 此题已通过的用户的集合中无此条 solution 对应的用户
			if (accepted_users.find(u_id) == accepted_users.end()) {
				switch (s_result) {
					case UnitedJudgeResult::SYSTEM_ERROR: // ignore system error
						break;
					case UnitedJudgeResult::ACCEPTED:
						accepted_users.insert(u_id);
						++__submit_num;
						break;
					default:
						++__submit_num;
						break;
				}
			}
		}
};

class UserSARecorder
{
		int __submit_num;
		std::set<ojv4::p_id_type> accepted_problems;

	public:
		UserSARecorder() :
			__submit_num(0), accepted_problems()
		{
		}

		int submit_num() const
		{
			return __submit_num;
		}

		int accept_num() const
		{
			return this->accepted_problems.size();
		}

		void add_solution(ojv4::p_id_type p_id, ojv4::s_result_enum_type s_result)
		{
			// 此用户已通过的题目编号的集合中无此条 solution 对应的题目编号
			if (accepted_problems.find(p_id) == accepted_problems.end()) {
				switch (s_result) {
					case UnitedJudgeResult::SYSTEM_ERROR: // ignore system error
						break;
					case UnitedJudgeResult::ACCEPTED:
						accepted_problems.insert(p_id);
						++__submit_num;
						break;
					default:
						++__submit_num;
						break;
				}
			}
		}
};

#endif /* SRC_MASTER_SARECORDER_HPP_ */
