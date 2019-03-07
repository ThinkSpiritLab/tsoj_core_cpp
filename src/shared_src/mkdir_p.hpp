/*
 * mkdir_p.hpp
 *
 *  Created on: 2018年11月7日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_MKDIR_P_HPP_
#define SRC_SHARED_SRC_MKDIR_P_HPP_

#include <cstdlib>
#include <stdexcept>
#include <string>

inline int mkdir_p(const char * path)
{
	int make_parent_path_return_value = system((std::string("mkdir -p ") += path).c_str());
	if (-1 == make_parent_path_return_value) {
		throw std::runtime_error("make path failed");
	}
	if (!WIFEXITED(make_parent_path_return_value) || WEXITSTATUS(make_parent_path_return_value)) {
		throw std::runtime_error("make path failed, exit status: " + std::to_string(WEXITSTATUS(make_parent_path_return_value)));
	}
	return make_parent_path_return_value;
}

inline int mkdir_p(const std::string & path)
{
	return mkdir_p(path.c_str());
}


#endif /* SRC_SHARED_SRC_MKDIR_P_HPP_ */
