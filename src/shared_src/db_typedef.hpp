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
#include <cstdint>
#include <chrono>
#include "united_resource.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/stadapter.h>


template <typename IntegerType>
class id_type_base
{
	public:
		using integer_type = IntegerType;

	protected:
		integer_type val;

	public:
		constexpr explicit id_type_base() : val()
		{
		}

		constexpr explicit id_type_base(integer_type val) : val(val)
		{
		}

		id_type_base(const mysqlpp::String & s) : val(s)
		{
		}

		id_type_base& operator=(const mysqlpp::String & s)
		{
			this->val = s;
			return *this;
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
};


template <typename IDType>
constexpr
typename std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type>, IDType>::value, bool>::type
operator==(const IDType & lhs, const IDType & rhs)
{
	return lhs.to_literal() == rhs.to_literal();
}

template <typename IDType>
constexpr
typename std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type>, IDType>::value, bool>::type
operator!=(const IDType & lhs, const IDType & rhs)
{
	return !(lhs == rhs);
}

template <typename IDType>
struct id_type_hash : std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type>, IDType>::value, std::true_type>::type
{
		using result_type = typename std::hash<typename IDType::integer_type>::result_type;
		using argument_type = IDType;

		result_type operator()(const argument_type & val) const
		{

			return std::hash<typename IDType::integer_type>()(val.to_literal());
		}
};

template <typename IDType>
struct literal_less : std::enable_if<std::is_base_of<id_type_base<typename IDType::integer_type>, IDType>::value, std::true_type>::type
{
		using result_type = typename std::hash<typename IDType::integer_type>::result_type;
		using argument_type = IDType;

		result_type operator()(const argument_type & a, const argument_type & b) const
		{

			return a.to_literal() < b.to_literal();
		}
};

namespace kerbal
{
	namespace redis
	{

	}
}

struct ojv4
{
		using u_id_literal = std::int32_t;
		class u_id_type : public id_type_base<u_id_literal>
		{
			public:
				using id_type_base<u_id_literal>::id_type_base;
		};

		using p_id_literal = std::int32_t;
		class p_id_type : public id_type_base<std::int32_t>
		{
			public:
				using id_type_base<std::int32_t>::id_type_base;
		};

		using c_id_literal = std::int32_t;
		class c_id_type : public id_type_base<c_id_literal>
		{
			public:
				using id_type_base<c_id_literal>::id_type_base;
		};

		using ct_id_literal = std::int32_t;
		class ct_id_type : public id_type_base<ct_id_literal>
		{
			public:
				using id_type_base<ct_id_literal>::id_type_base;
		};

		using s_id_literal = std::int32_t;
		class s_id_type : public id_type_base<s_id_literal>
		{
			public:
				using id_type_base<s_id_literal>::id_type_base;
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
	namespace redis
	{
		template <typename T>
		struct redis_type_traits;

		template<>
		struct redis_type_traits<ojv4::u_id_type> : public redis_type_traits<typename ojv4::u_id_type::integer_type>
		{
		};

		template<>
		struct redis_type_traits<ojv4::p_id_type> : public redis_type_traits<typename ojv4::p_id_type::integer_type>
		{
		};

		template<>
		struct redis_type_traits<ojv4::c_id_type> : public redis_type_traits<typename ojv4::c_id_type::integer_type>
		{
		};

		template<>
		struct redis_type_traits<ojv4::ct_id_type> : public redis_type_traits<typename ojv4::ct_id_type::integer_type>
		{
		};

		template<>
		struct redis_type_traits<ojv4::s_id_type> : public redis_type_traits<typename ojv4::s_id_type::integer_type>
		{
		};

	}

}

constexpr ojv4::c_id_type operator""_c_id(unsigned long long src)
{
	return ojv4::c_id_type(src);
}

constexpr ojv4::ct_id_type operator""_ct_id(unsigned long long src)
{
	return ojv4::ct_id_type(src);
}


#endif /* SRC_SHARED_SRC_DB_TYPEDEF_HPP_ */
