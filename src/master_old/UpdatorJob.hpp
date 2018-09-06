/*
 * UpdatorJob.hpp
 *
 *  Created on: 2018年7月23日
 *      Author: peter
 */

#ifndef SRC_JOB_GIVER_UPDATORJOB_HPP_
#define SRC_JOB_GIVER_UPDATORJOB_HPP_

#include "JobBase.hpp"
#include "Result.hpp"

#include "logger.hpp"

#include <kerbal/redis/redis_command.hpp>

using namespace kerbal::redis;

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>


class UpdatorJob : public JobBase
{
    private:
        typedef JobBase supper_t;
        mysqlpp::Connection mysqlConn;

    public:
        int uid; ///@brief user id
        int cid;
        std::string postTime; ///@brief post time
        bool haveAccepted; ///@brief whether the user has pass the problem before
        bool no_store_ac_code;
        bool is_rejudge; ///@brief is rejudge

        virtual void fetchDetailsFromRedis();

        UpdatorJob(int jobType, int sid, const kerbal::redis::RedisContext & redisConn,
                   const kerbal::mysql::MysqlContext & mysqlConn) :
                supper_t(jobType, sid, redisConn), mysqlConn(mysqlConn)
        {
        }


        /**
         * @brief Calculate this job's similarity
         * @return similarity of this job
         * @throw JobHandleException if any ERROR happened
         */
        int calculate_similarity() const;

        void store_code_to_accepted_solutions_dir() const;

        Result get_job_result();

		void set_cheat_status(std::chrono::seconds expire_time) const;

		void update_solution(const Result & result);
		void update_exercise_solution(const Result & result);
		void update_course_solution(const Result & result);
		void update_contest_solution(const Result & result);

		void update_source_code(const char * source_code);
		void update_compile_info();
		int update_user_and_problem(const Result & result);
		int update_course_user(const Result & result);
		int update_user_problem(int stat);
		int update_contest_user_problem(int is_ac);
		int core_update_failed_table(const Result & result);

		void rejudge(const Result & result);
		void rejudge_solution(const Result & result);
        void rejudge_user_and_problem(const Result & result, UnitedJudgeResult orig_result);
        void rejudge_course_user(const Result & result, UnitedJudgeResult orig_result);
        int rejudge_user_problem();
        int rejudge_contest_user_problem();
};


#endif /* SRC_JOB_GIVER_UPDATORJOB_HPP_ */
