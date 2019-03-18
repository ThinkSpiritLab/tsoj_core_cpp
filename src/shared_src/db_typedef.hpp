/*
 * db_typedef.hpp
 *
 *  Created on: 2018年11月6日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_DB_TYPEDEF_HPP_
#define SRC_SHARED_SRC_DB_TYPEDEF_HPP_

#include <kerbal/utility/storage.hpp>
#include <kerbal/redis/redis_type_traits.hpp>
#include <kerbal/redis_v2/type_traits.hpp>
#include <cstdint>
#include <chrono>
#include "united_resource.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/stadapter.h>


template <typename IntegerType, typename IDType>
class id_type_base
{
	public:
		using integer_type = IntegerType;

	protected:
		integer_type val;
		using supper_t = id_type_base<IntegerType, IDType>;

	public:
		constexpr explicit id_type_base() : val(0)
		{
		}

		constexpr explicit id_type_base(integer_type val) : val(val)
		{
		}

		id_type_base(const mysqlpp::String & s) : val(s)
		{
		}

		IDType& operator=(const mysqlpp::String & s)
		{
			this->val = s;
			return static_cast<IDType&>(*this);
		}

		constexpr explicit operator integer_type() const
		{
			return val;
		}

		operator mysqlpp::SQLTypeAdapter() const
		{
			return val;
		}

		constexpr integer_type to_literal() const
		{
			return val;
		}

		friend std::ostream& operator<<(std::ostream & out, const id_type_base & src)
		{
			out << src.val;
			return out;
		}

		friend std::istream& operator>>(std::istream & in, id_type_base & src)
		{
			in >> src.val;
			return in;
		}

		friend bool operator==(const IDType & lhs, const IDType & rhs)
		{
			return lhs.val == rhs.val;
		}

		friend bool operator!=(const IDType & lhs, const IDType & rhs)
		{
			return lhs.val != rhs.val;
		}

		struct hash : std::hash<integer_type>
		{
				using argument_type = IDType;

				auto operator()(const argument_type & val) const
				{
					return std::hash<integer_type>::operator()(val.val);
				}
		};

		struct literal_less
		{
				using argument_type = IDType;

				bool operator()(const argument_type & lhs, const argument_type & rhs) const
				{
					return lhs.val < rhs.val;
				}
		};
};

namespace std
{
	template <typename IDType>
	typename std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type, IDType>, IDType>::value,
	std::string>::type
	to_string(const IDType & id)
	{
		return std::to_string(id.to_literal());
	}
}

namespace boost
{
	template <typename IDType>
	typename std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type, IDType>, IDType>::value,
	IDType>::type
	lexical_cast(const char * s)
	{
		return boost::lexical_cast<typename IDType::integer_type>(s);
	}
}

struct ojv4
{
		using u_id_literal = std::int32_t;
		struct u_id_type : id_type_base<u_id_literal, u_id_type>
		{
				using supper_t::supper_t;
		};

		using p_id_literal = std::int32_t;
		struct p_id_type : id_type_base<p_id_literal, p_id_type>
		{
				using supper_t::supper_t;
		};

		using c_id_literal = std::int32_t;
		struct c_id_type : id_type_base<c_id_literal, c_id_type>
		{
				using supper_t::supper_t;
		};

		using ct_id_literal = std::int32_t;
		struct ct_id_type : id_type_base<ct_id_literal, ct_id_type>
		{
				using supper_t::supper_t;
		};

		using s_id_literal = std::int32_t;
		struct s_id_type : id_type_base<s_id_literal, s_id_type>
		{
				using supper_t::supper_t;
		};

		using s_result_in_db_type = std::int32_t;
		using s_result_enum = UnitedJudgeResult;

		using s_similarity_type = std::int16_t;

		using s_time_literal = std::int32_t;
		using s_time_in_milliseconds = std::chrono::duration<s_time_literal, std::milli>;

		template <typename Ratio>
		using s_time_type = std::chrono::duration<s_time_literal, Ratio>;

		using s_mem_literal = std::int32_t;
		using s_mem_in_byte = kerbal::utility::storage<s_mem_literal>;
		using s_mem_in_MB = kerbal::utility::storage<s_mem_literal, kerbal::utility::mebi>;

		template <typename Ratio>
		using s_mem_type = kerbal::utility::storage<s_mem_literal, Ratio>;

		/**
		 * @brief 枚举类，标识每道题目对一个用户的状态
		 */
		enum class u_p_status_enum
		{
			TODO = -1,
			ACCEPTED = 0,
			ATTEMPTED = 1,
		};

		using u_p_status_type = std::int8_t;
};

namespace kerbal
{
	namespace redis_v2
	{
		template <typename IDType>
		typename std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type, IDType>, IDType>::value,
		IDType>::type
		redis_type_cast(const char * src)
		{
			return redis_type_cast<typename IDType::interger_type>(src);
		}

		template <typename >
		struct is_redis_execute_allow_type;

		template <>
		struct is_redis_execute_allow_type<ojv4::u_id_type> : public is_redis_execute_allow_type<typename ojv4::u_id_type::integer_type>
		{
		};

		template <>
		struct is_redis_execute_allow_type<ojv4::p_id_type> : public is_redis_execute_allow_type<typename ojv4::p_id_type::integer_type>
		{
		};

		template <>
		struct is_redis_execute_allow_type<ojv4::c_id_type> : public is_redis_execute_allow_type<typename ojv4::c_id_type::integer_type>
		{
		};

		template <>
		struct is_redis_execute_allow_type<ojv4::ct_id_type> : public is_redis_execute_allow_type<typename ojv4::ct_id_type::integer_type>
		{
		};

		template <>
		struct is_redis_execute_allow_type<ojv4::s_id_type> : public is_redis_execute_allow_type<typename ojv4::s_id_type::integer_type>
		{
		};
	}
}

struct oj: public ojv4
{
};

constexpr oj::c_id_type operator""_c_id(unsigned long long src)
{
	return oj::c_id_type(src);
}

constexpr oj::ct_id_type operator""_ct_id(unsigned long long src)
{
	return oj::ct_id_type(src);
}

#endif /* SRC_SHARED_SRC_DB_TYPEDEF_HPP_ */
