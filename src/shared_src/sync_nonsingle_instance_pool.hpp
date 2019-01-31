/*
 * sync_nonsingle_instance_pool.hpp
 *
 *  Created on: 2018年12月29日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_SYNC_NONSINGLE_INSTANCE_POOL_HPP_
#define SRC_SHARED_SRC_SYNC_NONSINGLE_INSTANCE_POOL_HPP_

#include <deque>
#include <type_traits>
#include <thread>
#include <mutex>
#include <exception>

#include <kerbal/utility/noncopyable.hpp>


class resource_exhausted_exception : public std::runtime_error
{
	public:
		resource_exhausted_exception() :
					std::runtime_error("resource exhausted in instance pool")
		{
		}
};


template <typename InstanceType, typename PoolContainer = std::deque<InstanceType*> >
class sync_nonsingle_instance_pool : kerbal::utility::noncopyable, kerbal::utility::nonassignable
{
	private:
		PoolContainer instance_pool;
		std::mutex pool_vis_mtx;

		class __auto_revert_handle: kerbal::utility::noncopyable, kerbal::utility::nonassignable
		{
			private:
				InstanceType * ptr_to_instance;
				sync_nonsingle_instance_pool * ptr_to_pool;

				__auto_revert_handle() :
					ptr_to_instance(nullptr), ptr_to_pool(nullptr)
				{
				}

				__auto_revert_handle(InstanceType * ptr_to_instance, sync_nonsingle_instance_pool * ptr_to_pool) :
						ptr_to_instance(ptr_to_instance), ptr_to_pool(ptr_to_pool)
				{
				}

				friend __auto_revert_handle sync_nonsingle_instance_pool::fetch();
				friend __auto_revert_handle sync_nonsingle_instance_pool::sync_fetch(std::chrono::milliseconds);
				friend void sync_nonsingle_instance_pool::revert(__auto_revert_handle & handler);

			public:
				__auto_revert_handle(__auto_revert_handle && src) :
						ptr_to_instance(src.ptr_to_instance), ptr_to_pool(src.ptr_to_pool)
				{
					src.ptr_to_instance = nullptr;
				}

				~__auto_revert_handle()
				{
					if (this->ptr_to_instance == nullptr) {
						return;
					}
					ptr_to_pool->revert(*this);
				}

				__auto_revert_handle& operator=(__auto_revert_handle && src)
				{
					this->revert();
					this->ptr_to_instance = src.ptr_to_instance;
					src.ptr_to_instance = nullptr;
					this->ptr_to_pool = src.ptr_to_pool;
					src.ptr_to_pool = nullptr;
					return *this;
				}

				bool empty() const
				{
					return this->ptr_to_instance == nullptr;
				}

				InstanceType& operator*() const
				{
					return *ptr_to_instance;
				}

				InstanceType* operator->() const
				{
					return ptr_to_instance;
				}

				void revert()
				{
					ptr_to_pool->revert(*this);
				}

				void abandon()
				{
					delete ptr_to_instance;
					this->ptr_to_instance = nullptr;
					ptr_to_pool = nullptr;
				}
		};

		void revert(__auto_revert_handle & handler)
		{
			if (handler.ptr_to_instance == nullptr) {
				return;
			}

			InstanceType* p = handler.ptr_to_instance;
			handler.ptr_to_instance = nullptr;

			try {
				std::lock_guard<std::mutex> lck(pool_vis_mtx);
				instance_pool.push_back(p);
			} catch (...) {
				delete p;
				throw;
			}
		}

		void revert(__auto_revert_handle && handler)
		{
			if (handler.ptr_to_instance == nullptr) {
				return;
			}

			InstanceType* p = handler.ptr_to_instance;
			handler.ptr_to_instance = nullptr;

			try {
				std::lock_guard<std::mutex> lck(pool_vis_mtx);
				instance_pool.push_back(p);
			} catch (...) {
				delete p;
				throw;
			}
		}

	public:

		typedef __auto_revert_handle auto_revert_handle;
		typedef PoolContainer pool_type;
		typedef typename std::remove_reference<decltype(instance_pool.size())>::type size_type;
		
		~sync_nonsingle_instance_pool() noexcept
		{
			while (!instance_pool.empty()) {
				delete instance_pool.front();
				instance_pool.pop_front();
			}
		}

		void add(InstanceType* p)
		{
			try {
				std::lock_guard<std::mutex> lck(pool_vis_mtx);
				instance_pool.push_back(p);
			} catch (...) {
				delete p;
			}
		}

		template <typename ... Args>
		void emplace(Args&& ... args)
		{
			InstanceType* p = new InstanceType(std::forward<Args>(args)...);
			try {
				this->add(p);
			} catch (...) {
				delete p;
				throw;
			}
		}

		size_type size() const
		{
			return instance_pool.size();
		}

		auto_revert_handle fetch()
		{
			std::lock_guard<std::mutex> lck(pool_vis_mtx);
			while (!instance_pool.empty()) {
				InstanceType* p = instance_pool.front();
				instance_pool.pop_front();
				if (p != nullptr) {
					return auto_revert_handle(p, this);
				}
			}
			throw resource_exhausted_exception();
		}

		auto_revert_handle sync_fetch(std::chrono::milliseconds next_try_interval = std::chrono::milliseconds(5))
		{
			while (true) {
				{
					std::lock_guard<std::mutex> lck(pool_vis_mtx);
					while (!instance_pool.empty()) {
						InstanceType* p = instance_pool.front();
						instance_pool.pop_front();
						if (p != nullptr) {
							return auto_revert_handle(p, this);
						}
					}
				}
				std::this_thread::sleep_for(next_try_interval);
			}
		}
};

#endif /* SRC_SHARED_SRC_SYNC_NONSINGLE_INSTANCE_POOL_HPP_ */
