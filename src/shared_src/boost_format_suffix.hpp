/*
 * boost_format_suffix.hpp
 *
 *  Created on: 2018年8月7日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_BOOST_FORMAT_SUFFIX_HPP_
#define SRC_SHARED_SRC_BOOST_FORMAT_SUFFIX_HPP_

#include <boost/format.hpp>

struct format_helper
{
		boost::format templ;

		format_helper(const char * s) : templ(s)
		{
		}

		template <typename ... Args>
		boost::format& operator()(Args && ... args)
		{
			std::initializer_list<int> {(templ % args, 0)...};
			return templ;
		}
};


format_helper operator""_fmt(const char * s, size_t size)
{
	return format_helper(s);
}

#endif /* SRC_SHARED_SRC_BOOST_FORMAT_SUFFIX_HPP_ */
