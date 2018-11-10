/*
 * ojv4_type.hpp
 *
 *  Created on: 2018年11月6日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_OJV4_DB_TYPE_HPP_
#define SRC_SHARED_SRC_OJV4_DB_TYPE_HPP_

#include <kerbal/utility/storage.hpp>
#include <cstdint>
#include <chrono>
#include "united_resource.hpp"

struct ojv4
{
		using c_id_type = std::int32_t;
		using ct_id_type = std::int32_t;
		using s_id_type = std::int32_t;
		using u_id_type = std::int32_t;
		using p_id_type = std::int32_t;
		using s_result_in_db_type = std::int32_t;
		using s_result_enum_type = UnitedJudgeResult;
		using s_similarity_type = std::int8_t;
		using s_time_in_db_type = std::int32_t;
		using s_time_in_milliseconds = std::chrono::duration<s_time_in_db_type, std::milli>;
		using s_mem_in_db_type = std::int32_t;
		using s_mem_in_byte = kerbal::utility::storage<s_mem_in_db_type, std::ratio<1, 1> >;
		using s_mem_in_MB = kerbal::utility::storage<s_mem_in_db_type, std::ratio<1024 * 1024, 1> >;

};



#endif /* SRC_SHARED_SRC_OJV4_DB_TYPE_HPP_ */
