/*
 * process.hpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#ifndef SRC_PROCESS_HPP_
#define SRC_PROCESS_HPP_

#include <exception>
#include <cstdlib>

#include <signal.h>
#include <unistd.h>

class Process
{
	protected:
		pid_t father_id;
		pid_t child_id;
		bool _kill;
	public:
		template <typename _Callable, typename ... _Args>
		explicit Process(bool _kill, _Callable&& func, _Args&&... args) :
				father_id(getpid()), child_id(-1), _kill(_kill)
		{
			child_id = fork();
			if (child_id == -1) {
				throw std::exception();
			} else if (child_id == 0) {
				func(args...);
				exit(0);
			}
		}

		virtual ~Process() noexcept
		{
			if (_kill && getpid() == father_id) {
				kill(child_id, SIGKILL);
			}
		}
};

#endif /* SRC_PROCESS_HPP_ */
