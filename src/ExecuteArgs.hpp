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

class ExecuteArgs
{
	private:
		std::vector<std::string> args;

	public:
		ExecuteArgs();

		template<typename ForwardInteger>
		ExecuteArgs(ForwardInteger begin, ForwardInteger end) :
				args(begin, end)
		{
		}

		ExecuteArgs(std::initializer_list<std::string> list);

		ExecuteArgs& operator=(std::initializer_list<std::string> list);

		std::unique_ptr<char*[]> getArgs() const;
};

#endif /* SRC_EXECUTEARGS_HPP_ */
