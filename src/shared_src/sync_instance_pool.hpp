/*
 * sync_object_pool.hpp
 *
 *  Created on: 2018年11月8日
 *      Author: peter
 */

#ifndef SRC_SHARED_SRC_SYNC_INSTANCE_POOL_HPP_
#define SRC_SHARED_SRC_SYNC_INSTANCE_POOL_HPP_

#include <deque>
#include <thread>
#include <mutex>
#include <exception>

#include <kerbal/utility/noncopyable.hpp>

template <typename InstanceType>
class sync_instance_pool final : kerbal::utility::noncopyable, kerbal::utility::nonassignable
{
	private:
		sync_instance_pool() = delete;
		typedef std::deque<InstanceType*> pool_type;

		static pool_type& get_pool()
		{
			class singleton_pool_package : kerbal::utility::noncopyable, kerbal::utility::nonassignable
			{
				public:
					pool_type pool;

				private:
					singleton_pool_package()
					{
					}

					friend pool_type& sync_instance_pool::get_pool();

					~singleton_pool_package()
					{
						for(auto & conn : pool) {
							delete conn;
						}
					}
			};
			static singleton_pool_package pool_package;
			return pool_package.pool;
		}

		static std::mutex& get_mutex()
		{
			static std::mutex mtx;
			return mtx;
		}


	public:
		class auto_revert_t : kerbal::utility::noncopyable, kerbal::utility::nonassignable
		{
			private:
				friend auto_revert_t sync_instance_pool::fetch();
				friend auto_revert_t sync_instance_pool::block_fetch();
				friend void sync_instance_pool::revert(auto_revert_t&);

				InstanceType * ptr_to_conn;

				auto_revert_t(InstanceType * ptr_to_conn) : ptr_to_conn(ptr_to_conn)
				{
				}

			public:

				auto_revert_t(auto_revert_t && src) : ptr_to_conn(src.ptr_to_conn)
				{
//					cout << "move: " << ptr_to_conn << " from: " << &src << " to: " << this << endl;
					src.ptr_to_conn = nullptr;
				}

				~auto_revert_t()
				{
//					cout << "~auto_revert_t:  p: " << this->ptr_to_conn << " this: " << this << endl;
					if(this->ptr_to_conn != nullptr) {
						sync_instance_pool::revert(*this);
					}
				}

				InstanceType& operator*() const
				{
					return *ptr_to_conn;
				}

				InstanceType* operator->() const
				{
					return ptr_to_conn;
				}
		};

		template <typename ChildInstanceType = InstanceType, typename ... ConstructArgs>
		static std::pair<size_t, std::exception_ptr> construct(size_t num, ConstructArgs&& ... args) noexcept
		{
			std::lock_guard<std::mutex> lck (get_mutex());
			for(size_t i = 0; i < num; ++i) {
				try {
					get_pool().push_back(new ChildInstanceType(std::forward<ConstructArgs>(args)...));
				} catch(...) {
					return std::make_pair(i, std::current_exception());
				}
			}
			return std::make_pair(num, nullptr);
		}

		static auto_revert_t fetch()
		{
			std::lock_guard<std::mutex> lck (get_mutex());
			while(!get_pool().empty()) {
				InstanceType* p = get_pool().front();
				get_pool().pop_front();
				if(p != nullptr) {
					return auto_revert_t(p);
				}
			}
			throw std::runtime_error("empty instance pool error");
		}

		static auto_revert_t block_fetch()
		{
			while(true) {
				{
					std::lock_guard<std::mutex> lck (get_mutex());
					while(!get_pool().empty()) {
						InstanceType* p = get_pool().front();
						get_pool().pop_front();
						if(p != nullptr) {
							return auto_revert_t(p);
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}

		static void revert(auto_revert_t & conn)
		{
//			cout << "trying revert" << endl;
			std::lock_guard<std::mutex> lck (get_mutex());
//			cout << "revert" << endl;
			get_pool().push_back(conn.ptr_to_conn);
			conn.ptr_to_conn = nullptr;
		}
};




#endif /* SRC_SHARED_SRC_SYNC_INSTANCE_POOL_HPP_ */
