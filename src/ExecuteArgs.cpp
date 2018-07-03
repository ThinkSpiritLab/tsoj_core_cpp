/*
 * ExecuteArgs.cpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#include "ExecuteArgs.hpp"

ExecuteArgs::ExecuteArgs()
{
}

ExecuteArgs::ExecuteArgs(std::initializer_list<std::string> list) :
		args(list.begin(), list.end())
{
}

ExecuteArgs& ExecuteArgs::operator=(std::initializer_list<std::string> list)
{
	args.assign(list.begin(), list.end());
	return *this;
}

std::unique_ptr<char*[]> ExecuteArgs::getArgs() const
{
	typedef char * pointer_to_char;
	std::unique_ptr<pointer_to_char[]> res(new char*[args.size() + 1]);
	size_t i = 0;
	for (i = 0; i < args.size(); ++i) {
		res.get()[i] = const_cast<char*>(args[i].c_str());
	}
	res.get()[i] = NULL;
	return res;
}
