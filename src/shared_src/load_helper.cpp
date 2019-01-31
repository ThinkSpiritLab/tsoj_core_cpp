/*
 * load_helper.cpp
 *
 *  Created on: 2018年8月7日
 *      Author: peter
 */


#include <algorithm>
#include "load_helper.hpp"

std::pair<std::string, std::string> parse_buf(const std::string & buf)
{
	using std::string;

	const static std::pair<string, string> empty_args_pair;

	if (buf[0] == ';') {
		return empty_args_pair;
	}

	const string::const_iterator it_p = std::find(buf.begin(), buf.end(), '=');
	if (it_p == buf.end()) {
		return empty_args_pair;
	}

	string::const_iterator it_i = std::find_if_not(buf.begin(), buf.end(), isspace);

	string key, value;
	for (; it_i != it_p && !isspace(*it_i); ++it_i) {
		key.push_back(*it_i);
	}

	it_i = std::find_if_not(std::next(it_p), buf.end(), [](char c) {
		return isspace(c) || c == '\"';
	});

	for (; it_i != buf.end(); ++it_i) {
		if (isspace(*it_i) || *it_i == '\"') {
			break;
		}
		value.push_back(*it_i);
	}

	return std::make_pair(key, value);
}

