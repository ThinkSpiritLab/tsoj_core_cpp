/*
 * process.hpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#ifndef SRC_PROCESS_HPP_
#define SRC_PROCESS_HPP_

#include <utility>
#include <exception>
#include <cstdlib>

#include <signal.h>
#include <unistd.h>
#include <wait.h>

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
				{
					func(std::forward<_Args>(args)...);
				}
				exit(0);
			}
		}

		Process(const Process&) = delete;

		virtual ~Process() noexcept
		{
			if (_kill && getpid() == father_id) {
				::kill(child_id, SIGKILL);
			}
		}
		
		pid_t get_child_id() const
		{
			return child_id;
		}
		
		int kill(int __sig)
		{
			return ::kill(child_id, __sig);
		}

		pid_t wait4(int* __stat_loc, int __options, struct rusage *__usage) noexcept
		{
			return ::wait4(child_id, __stat_loc, __options, __usage);
		}
};

#endif /* SRC_PROCESS_HPP_ */
