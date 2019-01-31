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

#include <kerbal/utility/noncopyable.hpp>

class process : virtual kerbal::utility::noncopyable, kerbal::utility::nonassignable
{
	public:
		typedef pid_t pid_type;

	protected:
		pid_type father_id;
		pid_type child_id;

		enum
		{
			none, joined, detached
		} status;

	public:
		process() noexcept :
				father_id(0), child_id(0), status(none)
		{
		}

		template <typename Callable, typename ... Args>
		explicit process(Callable && func, Args && ... args) :
				father_id(getpid()), child_id(-1), status(joined)
		{
			child_id = fork();
			if (child_id == -1) {
				status = none;
				throw std::exception();
			} else if (child_id == 0) {
				{
					func(std::forward<Args>(args)...);
				}
				exit(0);
			}
		}

		~process() noexcept
		{
			if (getpid() == father_id) {
				switch (status) {
					case none:
						break;
					case joined:
						this->kill(SIGKILL);
						break;
					case detached:
						break;
				}
			}
		}

		pid_type get_father_id() const noexcept
		{
			return this->father_id;
		}

		pid_type get_child_id() const noexcept
		{
			return this->child_id;
		}

		process(process && src) noexcept :
				father_id(src.father_id), child_id(src.child_id), status(src.status)
		{
		}

		void swap(process & with) noexcept
		{
			std::swap(this->father_id, with.father_id);
			std::swap(this->child_id, with.child_id);
			std::swap(this->status, with.status);
		}

		process& operator=(process&& src) noexcept
		{
			switch (status) {
				case none:
					this->swap(src);
					break;
				case joined:
					this->kill(SIGKILL);
					this->father_id = src.father_id;
					src.father_id = 0;
					this->child_id = src.child_id;
					src.child_id = 0;
					this->status = src.status;
					src.status = none;
					break;
				case detached:
					this->father_id = src.father_id;
					src.father_id = 0;
					this->child_id = src.child_id;
					src.child_id = 0;
					this->status = src.status;
					src.status = none;
					break;
			}
			return *this;
		}

		bool joinable() const noexcept
		{
			return status == joined;
		}

		pid_type join() noexcept
		{
			if (status == none) {
				return 0;
			}
			return this->join(nullptr, 0);
		}

		/**
		 * @brief Wait for a child matching PID to die.
		 * @param status_loc The location where the process status will be put.
		 * @return If the WNOHANG bit is set in OPTIONS, and that child is not already dead, return (pid_t) 0.
		 * 			If successful, return PID and store the dead child's status in STAT_LOC.
		 *  		Return (pid_t) -1 for errors.
		 *  		If the WUNTRACED bit is set in OPTIONS, return status for stopped children; otherwise don't.
		 *
		 */
		pid_type join(int *stat_loc, int options) noexcept
		{
			if (status == none) {
				return 0;
			}
			pid_type res = ::waitpid(this->child_id, stat_loc, options);
			if (res == -1) {
				return res;
			}
			this->father_id = 0;
			this->child_id = 0;
			this->status = none;
			return res;
		}

		/**
		 * @brief Wait for the process to exit. Put the status in *status_loc
		 * @param status_loc The location where the process status will be put.
		 * @param options If the WUNTRACED bit is set in OPTIONS, return status for stopped children; otherwise don't.
		 * @param usage If not nil, store information about the child's resource usage there.
		 * @return For errors return (pid_type) (-1); otherwise return the process ID.
		 */
		pid_type join(int * status_loc, int options, struct rusage * usage) noexcept
		{
			if (status == none) {
				return 0;
			}
			return ::wait4(child_id, status_loc, options, usage);
		}

		void detach() noexcept
		{
			status = detached;
		}

		/**
		 * @brief Send signal SIG to the process.
		 * @param sig signal send to child process.
		 * @return If success return 0, For errors, return other value.
		 */
		int kill(int sig) noexcept
		{
			if (status == none) {
				return 0;
			}
			int res = ::kill(child_id, sig);
			if (res != 0) {
				return res;
			}
			this->father_id = 0;
			this->child_id = 0;
			this->status = none;
			return res;
		}

};

#endif /* SRC_PROCESS_HPP_ */
