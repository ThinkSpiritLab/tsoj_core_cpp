/*
 * cptr.hpp
 *
 *  Created on: 2018年8月16日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_CPTR_HPP_
#define SRC_SHARED_SRC_CPTR_HPP_

#include <cstddef>

constexpr const char * operator""_cptr(const char * s, size_t len)
{
	return s;
}

#endif /* SRC_SHARED_SRC_CPTR_HPP_ */
