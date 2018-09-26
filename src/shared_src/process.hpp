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
	public:
		typedef pid_t pid_type;

	protected:
		pid_type father_id;
		pid_type child_id;
		bool _kill;

	public:
		template <typename Callable, typename ... Args>
		explicit Process(bool _kill, Callable && func, Args && ... args) :
				father_id(getpid()), child_id(-1), _kill(_kill)
		{
			child_id = fork();
			if (child_id == -1) {
				throw std::exception();
			} else if (child_id == 0) {
				{
					func(std::forward<Args>(args)...);
				}
				exit(0);
			}
		}

		Process(const Process &) = delete;

		virtual ~Process() noexcept
		{
			if (_kill && getpid() == father_id) {
				this->kill(SIGKILL);
			}
		}

		pid_type get_child_id() const
		{
			return child_id;
		}

		/**
		 * @brief Send signal SIG to process number PID.
		 * @param sig signal send to child process.
		 * @return If success return 0, For errors, return other value.
		 */
		int kill(int sig) noexcept
		{
			return ::kill(child_id, sig);
		}

		/**
		 * @brief Wait for a child to die.
		 * @return When one does, return its process ID.
		 *         For errors, return (pid_type) -1.
		 */
		static pid_type wait() noexcept
		{
			return ::wait(nullptr);
		}

		/**
		 * @brief Wait for a child to die.
		 * @param status_loc pointer to the variable to store child's status.
		 * @return When one does, put its status in *STAT_LOC and return its process ID.
		 *         For errors, return (pid_type) -1.
		 */
		static pid_type wait(int * status_loc) noexcept
		{
			return ::wait(status_loc);
		}

		/**
		 * @brief Wait for a child to exit.
		 * @details When one does, put its status in *STAT_LOC and return its process ID.
		 *          For errors return (pid_t) -1.
		 *          If USAGE is not nil, store information about the child's resource usage there.
		 *          If the WUNTRACED bit is set in OPTIONS, return status for stopped children; otherwise don't.
		 * @return
		 */
		static pid_type wait3(int * status_loc, int options, struct rusage * usage)
		{
			return ::wait3(status_loc, options, usage);
		}

		pid_type wait4(int * status_loc, int options, struct rusage * usage) noexcept
		{
			return ::wait4(child_id, status_loc, options, usage);
		}
};

#endif /* SRC_PROCESS_HPP_ */
