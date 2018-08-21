/*
 * cptr.hpp
 *
 *  Created on: 2018年8月16日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_CPTR_HPP_
#define SRC_SHARED_SRC_CPTR_HPP_

#include <cstddef>

/**
 * @brief
 * @param s
 * @param len
 * @return
 * @details
 *  <h4> why need cptr? </h4>
 *
 *  template &lt;typename T&gt;<br>
 *  void f(const T & src)<br>
 *  {<br>
 *  &nbsp;&nbsp;&nbsp;&nbsp;cout &lt;&lt; src;<br>
 *  }<br>
 *
 *  f("abc");<br>
 *  f("abcd");
 *
 *
 *  In the previous example, the C++ compiler will generate two different functions if there are
 *  two instances trying to pass parameters with different length. That will lead to program size
 *  expansion and expand the compile time. An idea may have come to you that you can use
 *  f((const char *)"abc") and f((const char *)"abcd") to solve this problem. However, this
 *  solution is not convenient.
 *
 *  In C++11, we can take use of the feature of literal operator, which allow you to program in the way
 *  like f("abc"_cptr)
 *
 *  After a series of test, the program size of ojv4.judge_core.slave decrease by at least 15% in
 *  configuration of release. The optimization effect is obvious.
 *
 */
constexpr const char * operator""_cptr(const char * s, size_t len)
{
	return s;
}

#endif /* SRC_SHARED_SRC_CPTR_HPP_ */
