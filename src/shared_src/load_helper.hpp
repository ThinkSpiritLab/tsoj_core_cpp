/*
 * load_helper.hpp
 *
 *  Created on: 2018年8月7日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_LOAD_HELPER_HPP_
#define SRC_SHARED_SRC_LOAD_HELPER_HPP_

#include <exception>
#include <unordered_map>
#include <functional>
#include <string>

std::pair<std::string, std::string> parse_buf(const std::string & buf);

template <typename Type, typename ... Args>
class LoadConfig;

template <typename Type>
class Package: public std::exception
{
	public:
		std::function<Type(std::string)> type_caster;

	public:
		Package(std::function<Type(std::string)> type_caster) :
				type_caster(type_caster)
		{
		}
};

template <typename Type>
class LoadConfig<Type>
{
	protected:
		typedef std::unordered_map<std::string, std::pair<void*, std::exception_ptr> > map_t;
		map_t m;

	public:
		void add_rules(Type & arg, const std::string & key, std::function<Type(std::string)> type_caster)
		{
			m[key] = std::make_pair((void*) (&arg), std::make_exception_ptr(Package<Type>(type_caster)));
		}

		bool parse(const std::string & key, const std::string & value)
		{
			map_t::const_iterator it = m.find(key);
			if (it != m.cend()) {
				parse(it, value);
				return true;
			} else {
				return false;
			}
		}

	protected:
		void parse(map_t::const_iterator it, const std::string & value)
		{
			const auto & value_ptr = it->second.first;
			const auto & type_caster = it->second.second;
			try {
				std::rethrow_exception(type_caster);
			} catch (const Package<Type> & e) {
				*(Type*) value_ptr = e.type_caster(value);
			}
		}
};

template <typename Type, typename ... Args>
class LoadConfig: public LoadConfig<Args...>
{
	private:
		typedef LoadConfig<Args...> supper_t;

	protected:
		using supper_t::m;

	public:
		using supper_t::add_rules;

		void add_rules(Type & arg, const std::string & key, std::function<Type(std::string)> type_caster)
		{
			m[key] = std::make_pair((void*) (&arg), std::make_exception_ptr(Package<Type>(type_caster)));
		}

		bool parse(const std::string & key, const std::string & value)
		{
			typename supper_t::map_t::const_iterator it = m.find(key);
			if (it != m.cend()) {
				parse(it, value);
				return true;
			} else {
				return false;
			}
		}

	protected:
		void parse(typename supper_t::map_t::const_iterator it, const std::string & value)
		{
			const auto & value_ptr = it->second.first;
			const auto & type_caster = it->second.second;
			try {
				std::rethrow_exception(type_caster);
			} catch (const Package<Type> & e) {
				*(Type*) value_ptr = e.type_caster(value);
			} catch (...) {
				supper_t::parse(it, value);
			}
		}
};


#endif /* SRC_SHARED_SRC_LOAD_HELPER_HPP_ */
