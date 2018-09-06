/*
 * UpdateJobBase.hpp
 *
 *  Created on: 2018年9月4日
 *      Author: peter
 */

#ifndef SRC_MASTER_UPDATEJOBBASE_HPP_
#define SRC_MASTER_UPDATEJOBBASE_HPP_

#include "JobBase.hpp"
#include "Result.hpp"
#include "mysql_empty_res_exception.hpp"

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>

class UpdateJobBase: public JobBase
{
    private:
        typedef JobBase supper_t;

    protected:
        template <typename T> using unique_ptr = std::unique_ptr<T>;

        using RedisContext = kerbal::redis::RedisContext;

        unique_ptr<mysqlpp::Connection> mysqlConn;

        int uid; ///@brief user id
        int cid;
        std::string postTime; ///@brief post time
        bool haveAccepted; ///@brief whether the user has pass the problem before
        bool no_store_ac_code;
        bool is_rejudge; ///@brief is rejudge

    private:
        virtual void fetchDetailsFromRedis() override;

    protected:
        friend
		unique_ptr<UpdateJobBase>
        make_update_job(int jobType, int sid, const RedisContext & redisConn,
        				unique_ptr<mysqlpp::Connection> && mysqlConn);

		UpdateJobBase(int jobType, int sid, const RedisContext & redisConn,
				unique_ptr<mysqlpp::Connection> && mysqlConn);
    public:
		void handle() override;

    protected:
		virtual void update_solution(const Result & result) = 0;
		void update_source_code(const char * source_code) final;
		void update_compile_info(const char * compile_info) final;
		kerbal::redis::RedisReply get_source_code() const final;
		kerbal::redis::RedisReply get_compile_info() const final;
		virtual void update_user_problem(int is_ac) = 0;
		void core_update_failed_table(const Result & result) noexcept final;

    public:
		virtual ~UpdateJobBase() = default;
};

#endif /* SRC_MASTER_UPDATEJOBBASE_HPP_ */
