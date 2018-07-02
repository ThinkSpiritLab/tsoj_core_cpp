/*
 * ExecuteArgs.hpp
 *
 *  Created on: 2018年7月1日
 *      Author: peter
 */

#ifndef SRC_EXECUTEARGS_HPP_
#define SRC_EXECUTEARGS_HPP_

#include <vector>
#include <string>
#include <memory>

#include <iostream>

class ExecuteArgs
{
	private:
		std::vector<std::string> args;

	public:
		ExecuteArgs()
		{

		}

		template<typename ForwardInteger>
		ExecuteArgs(ForwardInteger begin, ForwardInteger end) :
				args(begin, end)
		{
		}

		ExecuteArgs(std::initializer_list<std::string> list) :
				args(list.begin(), list.end())
		{
		}

		ExecuteArgs& operator=(std::initializer_list<std::string> list)
		{
			args.assign(list.begin(), list.end());
			return *this;
		}

		std::unique_ptr<char*[]> getArgs() const
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
};

#endif /* SRC_EXECUTEARGS_HPP_ */
