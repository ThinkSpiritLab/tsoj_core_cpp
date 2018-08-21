/*
 * JobInfo.cpp
 *
 *  Created on: 2018年6月15日
 *      Author: peter
 */

#include "../slave/JudgeJob.hpp"

#include "logger.hpp"
#include "process.hpp"

#include <thread>
#include <algorithm>

#include <cstring>

#include <dirent.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <kerbal/redis/redis_data_struct/list.hpp>
#include <kerbal/redis/redis_type_cast.hpp>

#include "compare.hpp"
#include "Config.hpp"
#include "global_shared_variable.hpp"
#include "boost_format_suffix.hpp"
#include <kerbal/compatibility/chrono_suffix.hpp>

using namespace kerbal::redis;
using kerbal::redis::RedisContext;


JudgeJob::JudgeJob(int jobType, int sid, const kerbal::redis::RedisContext & conn) :
        supper_t(jobType, sid, conn)
{
    static boost::format dir_templ(init_dir + "/job-%d-%d");
    this->dir = (dir_templ % jobType % sid).str();
}

void JudgeJob::judge_job()
{
    LOG_DEBUG(jobType, sid, log_fp, "judge start");

    this->change_job_dir();
    this->storeSourceCode();

    LOG_DEBUG(jobType, sid, log_fp, "store source code finished");

    // compile
    this->commitJudgeStatusToRedis(JudgeStatus::COMPILING);
    LOG_DEBUG(jobType, sid, log_fp, "compile start");
    Result compile_result = this->compile();

    LOG_DEBUG(jobType, sid, log_fp, "compile finished");
    LOG_DEBUG(jobType, sid, log_fp, "compile result: ", compile_result);

    switch (compile_result.judge_result) {
        case UnitedJudgeResult::ACCEPTED: {
            LOG_DEBUG(jobType, sid, log_fp, "compile success");
            Result result = this->running();
            LOG_DEBUG(jobType, sid, log_fp, "judging finished");
            this->commitJudgeResultToRedis(result);
            break;
        }
        case UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED:
            LOG_WARNING(jobType, sid, log_fp, "compile resource limit exceed"_cptr);
            this->commitJudgeResultToRedis(compile_result);
            break;
        case UnitedJudgeResult::SYSTEM_ERROR:
            LOG_DEBUG(jobType, sid, log_fp, "system error");
            this->commitJudgeResultToRedis(compile_result);
            break;
        case UnitedJudgeResult::COMPILE_ERROR:
        default:
            LOG_DEBUG(jobType, sid, log_fp, "compile failed");
            this->commitJudgeResultToRedis(compile_result);

            if (this->set_compile_info() == false) {
                LOG_WARNING(jobType, sid, log_fp, "An error occurred while getting compile info."_cptr);
            }
            break;
    }

    //update update_queue
    static RedisCommand update_queue("rpush update_queue %d,%d");
    update_queue.execute(redisConn, jobType, sid);

#ifndef DEBUG
    // clear the dir
    this->clean_job_dir();
#endif

}

void JudgeJob::fetchDetailsFromRedis()
{
    this->supper_t::fetchDetailsFromRedis();
}

Result JudgeJob::running()
{
    RunningConfig config(*this);
//	this->commitJudgeStatusToRedis(conn, "cases_finished", 0);

    Result result;
    this->commitJudgeStatusToRedis(JudgeStatus::RUNNING);
    for (int i = 1; i <= cases; ++i) {
        // change input path
        std::string input_path = "%s/%d/in.%d"_fmt(input_dir, pid, i).str();
        config.input_path = input_path.c_str();

        LOG_DEBUG(jobType, sid, log_fp, "running case: ", i);
        Result current_running_result = this->execute(config);
        LOG_DEBUG(jobType, sid, log_fp, "running case: ", i, " finished; result: ", current_running_result);

        if (current_running_result.judge_result != UnitedJudgeResult::ACCEPTED) {
            return current_running_result;
        }

        // change answer path
        std::string answer = "%s/%d/out.%d"_fmt(input_dir, pid, i).str();
        this->commitJudgeStatusToRedis(JudgeStatus::JUDGING);
        // compare
        LOG_DEBUG(jobType, sid, log_fp, "comparing case: ", i);
        UnitedJudgeResult compare_result = compare(answer.c_str(), config.output_path);
        LOG_DEBUG(jobType, sid, log_fp, "comparing case: ", i, " finished; compare result: ", compare_result);
        if (compare_result != UnitedJudgeResult::ACCEPTED) {
            current_running_result.judge_result = compare_result;
            return current_running_result;
        }

        result = current_running_result;

        //set_judge_status(conn, type, job_id, "cases_finished", i);
    }
    LOG_DEBUG(jobType, sid, log_fp, "all cases accepted");
    return result;
}

namespace
{
    void
    staticCommitJudgeResultToRedis(int jobType, int sid, const kerbal::redis::RedisContext redisConn, const Result & result)
    {
        static boost::format judge_status_name_tmpl("judge_status:%d:%d");
        Operation opt(redisConn);
        try {
            opt.hmset((judge_status_name_tmpl % jobType % sid).str(),
                      "status"_cptr, (int) JudgeStatus::FINISHED,
                      "result"_cptr, (int) result.judge_result,
                      "cpu_time"_cptr, result.cpu_time.count(),
                      "real_time"_cptr, result.real_time.count(),
                      "memory"_cptr, (kerbal::utility::Byte(result.memory)).count(),
                      "similarity_percentage"_cptr, 0,
                      "judge_server_id"_cptr, judge_server_id);
        } catch (const std::exception & e) {
            LOG_FATAL(jobType, sid, log_fp, "Commit judge result failed. Error information: "_cptr, e.what(),
                      "; judge result: "_cptr, result);
            throw;
        }
    }
}

void JudgeJob::commitJudgeResultToRedis(const Result & result)
{
    staticCommitJudgeResultToRedis(jobType, sid, redisConn, result);
}

bool JudgeJob::set_compile_info() noexcept
{
    std::ifstream fin("compiler.out", std::ios::in);
    if (!fin) {
        LOG_FATAL(jobType, sid, log_fp, "Cannot open compiler.out"_cptr);
        return false;
    }

    // get length of file:
    fin.seekg(0, fin.end);
    int length = fin.tellg();
    fin.seekg(0, fin.beg);

    constexpr int MYSQL_TEXT_MAX_SIZE = 65535;
    char buffer[MYSQL_TEXT_MAX_SIZE + 10];
    fin.read(buffer, MYSQL_TEXT_MAX_SIZE);

    if (fin.bad()) {
        LOG_FATAL(jobType, sid, log_fp, "Read compile info error, only "_cptr, fin.gcount(), " could be read"_cptr);
        LOG_FATAL(jobType, sid, log_fp, "The read buffer: "_cptr, buffer);
        return false;
    }

    if (MYSQL_TEXT_MAX_SIZE >= 3 && length > MYSQL_TEXT_MAX_SIZE) {
        char * end = buffer + MYSQL_TEXT_MAX_SIZE - 3;
        for (int i = 0; i < 3; ++i) {
            *end = '.';
            ++end;
        }
    }

    try {
        static boost::format compile_info("compile_info:%d:%d");
        Operation opt(redisConn);
        opt.set((compile_info % jobType % sid).str(), buffer);
    } catch (const std::exception & e) {
        LOG_FATAL(jobType, sid, log_fp, "Set compile info failed. Error information: "_cptr, e.what());
        return false;
    }

    return true;
}

void JudgeJob::change_job_dir() const
{
    // TODO
    LOG_DEBUG(jobType, sid, log_fp, "make dir: "_cptr, dir);
    mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    chmod(dir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    if (chdir(dir.c_str())) {
        LOG_FATAL(jobType, sid, log_fp, "can not change to job dir: ["_cptr, dir, "]"_cptr);
        throw JobHandleException("can not change to job dir");
    }
}

void JudgeJob::clean_job_dir() const noexcept
{
    LOG_DEBUG(jobType, sid, log_fp, "clean dir"_cptr);
    std::unique_ptr<DIR, int (*)(DIR *)> dp(opendir("."), closedir);
    if (dp == nullptr) {
        LOG_FATAL(jobType, sid, log_fp, "clean dir failed."_cptr);
        return;
    }

    for (struct dirent * entry = nullptr; entry = readdir(dp.get()), entry != nullptr;) {
        if (strcmp(".", entry->d_name) != 0 && strcmp("..", entry->d_name) != 0) {
            unlink(entry->d_name);
        }
    }
    if (chdir("..")) {
        LOG_FATAL(jobType, sid, log_fp, "can not go back to ../"_cptr);
        return;
    }
    rmdir(dir.c_str());
}

namespace
{
    std::chrono::milliseconds timevalToChrono(const timeval & val)
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(seconds(val.tv_sec) + microseconds(val.tv_usec));
    }
}


Result JudgeJob::execute(const Config & config) const noexcept
{
    using namespace std::chrono;
    using namespace kerbal::compatibility::chrono_suffix;

    Result result;

    // check whether current user is root
    if (getuid() != 0) {
        LOG_FATAL(jobType, sid, log_fp, "ROOT_REQUIRED"_cptr);
        result.setErrorCode(RunnerError::ROOT_REQUIRED);
        return result;
    }

    // check args
    if (config.check_is_valid_config() == false) {
        LOG_FATAL(jobType, sid, log_fp, "INVALID_CONFIG"_cptr);
        result.setErrorCode(RunnerError::INVALID_CONFIG);
        return result;
    }

    std::unique_ptr<Process> child_process;

    try {
        child_process.reset(new Process(true, [this, &config]() {
            this->child_process(config);
        }));
    } catch (const std::exception & e) {
        LOG_FATAL(jobType, sid, log_fp, "Fork failed. Error information: "_cptr, e.what());
        result.setErrorCode(RunnerError::FORK_FAILED);
        return result;
    }

    // record current time
    time_point<high_resolution_clock> start(high_resolution_clock::now());

    if (config.limitRealTime()) {
        try {
            // new thread to monitor process running time
            std::thread timeout_killer_thread([&child_process](milliseconds timeout) {
                std::this_thread::sleep_for(timeout + 1_s);
                if (child_process->kill(SIGKILL) != 0) {
                    //TODO log
                    return;
                }
                return;
            }, config.max_real_time);
            try {
                timeout_killer_thread.detach();
            } catch (const std::system_error & e) {
                child_process->kill(SIGKILL);
                LOG_FATAL(jobType, sid, log_fp, "Thread detach failed. Error information: "_cptr, e.what(),
                          " , error code: "_cptr, e.code().value());
                result.setErrorCode(RunnerError::PTHREAD_FAILED);
                return result;
            }
        } catch (const std::system_error & e) {
            child_process->kill(SIGKILL);
            LOG_FATAL(jobType, sid, log_fp, "Thread construct failed. Error information: "_cptr, e.what(),
                      " , error code: "_cptr, e.code().value());
            result.setErrorCode(RunnerError::PTHREAD_FAILED);
            return result;
        } catch (...) {
            child_process->kill(SIGKILL);
            LOG_FATAL(jobType, sid, log_fp, "Thread construct failed. Error information: "_cptr,
                      "unknown exception"_cptr);
            result.setErrorCode(RunnerError::PTHREAD_FAILED);
            return result;
        }
        LOG_DEBUG(jobType, sid, log_fp, "Timeout thread work success");
    }

    int status;
    struct rusage resource_usage;

    // wait for child process to terminate
    // on success, returns the process ID of the child whose state has changed;
    // On error, -1 is returned.
    if (child_process->wait4(&status, WSTOPPED, &resource_usage) == -1) {
        child_process->kill(SIGKILL);
        LOG_FATAL(jobType, sid, log_fp, "WAIT_FAILED"_cptr);
        result.setErrorCode(RunnerError::WAIT_FAILED);
        return result;
    }

    result.real_time = duration_cast<milliseconds>(high_resolution_clock::now() - start);
    result.cpu_time = timevalToChrono(resource_usage.ru_utime) + timevalToChrono(resource_usage.ru_stime);
    result.memory = kerbal::utility::KB(resource_usage.ru_maxrss);
    result.exit_code = WEXITSTATUS(status);

    if (result.exit_code != 0) {
        LOG_WARNING(jobType, sid, log_fp, "exit code != 0"_cptr);
        result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
        return result;
    }
    // if signaled
    if (WIFSIGNALED(status) != 0) {
        result.signal = WTERMSIG(status);
        LOG_DEBUG(jobType, sid, log_fp, "signal: ", result.signal);
        switch (result.signal) {
            case SIGSEGV:
                if (config.limitMemory() && result.memory > config.max_memory) {
                    result.judge_result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
                } else {
                    result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
                }
                break;
            case SIGUSR1:
                result.judge_result = UnitedJudgeResult::SYSTEM_ERROR;
                break;
            default:
                result.judge_result = UnitedJudgeResult::RUNTIME_ERROR;
        }
    } else {
        if (config.limitMemory() && result.memory > config.max_memory) {
            result.judge_result = UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED;
        }
    }
    if (config.limitRealTime() && result.real_time > config.max_real_time) {
        result.judge_result = UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED;
    }
    if (config.limitCPUTime() && result.cpu_time > config.max_cpu_time) {
        result.judge_result = UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED;
    }
    return result;
}

Result JudgeJob::compile() const noexcept
{
    Result result = this->execute(CompileConfig(*this));

    switch (result.judge_result) {
        case UnitedJudgeResult::ACCEPTED:
            return result;
        case UnitedJudgeResult::CPU_TIME_LIMIT_EXCEEDED:
        case UnitedJudgeResult::REAL_TIME_LIMIT_EXCEEDED:
        case UnitedJudgeResult::MEMORY_LIMIT_EXCEEDED:
            result.judge_result = UnitedJudgeResult::COMPILE_RESOURCE_LIMIT_EXCEED;
            return result;
        case UnitedJudgeResult::RUNTIME_ERROR:
            result.judge_result = UnitedJudgeResult::COMPILE_ERROR;
            return result;
        case UnitedJudgeResult::SYSTEM_ERROR:
            LOG_FATAL(jobType, sid, log_fp, "System error occurred while compiling. Execute result: "_cptr, result);
            return result;
        default:
            LOG_FATAL(jobType, sid, log_fp, "Unexpected compile result: "_cptr, result);
            result.judge_result = UnitedJudgeResult::COMPILE_ERROR;
            return result;
    }
}

bool JudgeJob::child_process(const Config & config) const
{
    struct rlimit max_stack;
    max_stack.rlim_cur = max_stack.rlim_max = (rlim_t) (config.max_stack.count());
    if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
        LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
        raise(SIGUSR1);
        return false;
    }

    // set memory limit
    if (config.limitMemory()) {
        struct rlimit max_memory;
        max_memory.rlim_cur = max_memory.rlim_max = (rlim_t) (config.max_memory.count()) * 2;
        if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
            raise(SIGUSR1);
            return false;
        }
    }

    // set cpu time limit (in seconds)
    if (config.limitCPUTime()) {
        struct rlimit max_cpu_time;
        max_cpu_time.rlim_cur = max_cpu_time.rlim_max = (rlim_t) ((config.max_cpu_time.count() + 1000) / 1000);
        if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
            raise(SIGUSR1);
            return false;
        }
    }

    // set max process number limit
    if (config.limitProcessNumber()) {
        struct rlimit max_process_number;
        max_process_number.rlim_cur = max_process_number.rlim_max = (rlim_t) config.max_process_number;
        if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
            raise(SIGUSR1);
            return false;
        }
    }

    // set max output size limit
    if (config.limitOutput()) {
        struct rlimit max_output_size;
        max_output_size.rlim_cur = max_output_size.rlim_max = (rlim_t) config.max_output_size.count();
        if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::SETRLIMIT_FAILED);
            raise(SIGUSR1);
            return false;
        }
    }

    std::unique_ptr<FILE, int (*)(FILE *)> input_file(fopen(config.input_path, "r"), fclose);
    if (config.input_path != nullptr) {
        if (!input_file) {
            LOG_FATAL(jobType, sid, log_fp, "can not open \""_cptr, config.input_path, "\""_cptr);
            raise(SIGUSR1);
            return false;
        }
        // redirect file -> stdin
        // On success, these system calls return the new descriptor.
        // On error, -1 is returned, and errno is set appropriately.
        if (dup2(fileno(input_file.get()), fileno(stdin)) == -1) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED, "dup2 input file failed"_cptr);
            raise(SIGUSR1);
            return false;
        }
    }

    /*
     *  Out      |  Err
     *  -------------------------
     *  null     |  null
     *  null     |  err.txt
     *  out.txt  |  null
     *  out.txt  |  out.txt (DO NOT open twice)
     *  out.txt  |  err.txt
     */
    std::shared_ptr<FILE> output_file = nullptr, error_file = nullptr;

    if (config.output_path != nullptr && config.error_path != nullptr &&
        strcmp(config.output_path, config.error_path) == 0) {
        // 标准输出与错误输出指定为同一个文件
        auto of_ptr = fopen(config.output_path, "w"); //DO NOT fclose this ptr while exit brace
        if (!of_ptr) {
            LOG_FATAL(jobType, sid, log_fp, "can not open \""_cptr, config.output_path, "\""_cptr);
            raise(SIGUSR1);
            return false;
        } else {
            output_file.reset(of_ptr, fclose);
            error_file = output_file;
        }

    } else {
        if (config.output_path != nullptr) {
            auto of_ptr = fopen(config.output_path, "w"); //DO NOT fclose this ptr while exit brace
            if (!of_ptr) {
                LOG_FATAL(jobType, sid, log_fp, "can not open \""_cptr, config.output_path, "\""_cptr);
                raise(SIGUSR1);
                return false;
            } else {
                output_file.reset(of_ptr, fclose);
            }
        }

        if (config.error_path != nullptr) {
            auto ef_ptr = fopen(config.error_path, "w"); //DO NOT fclose this ptr while exit brace
            if (!ef_ptr) {
                LOG_FATAL(jobType, sid, log_fp, "can not open \""_cptr, config.error_path, "\""_cptr);
                raise(SIGUSR1);
                return false;
            } else {
                error_file.reset(ef_ptr, fclose);
            }
        }
    }

    // redirect stdout -> file
    if (output_file != nullptr) {
        if (dup2(fileno(output_file.get()), fileno(stdout)) == -1) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED, "dup2 output file failed"_cptr);
            raise(SIGUSR1);
            return false;
        }
    }
    if (error_file != nullptr) {
        // redirect stderr -> file
        if (dup2(fileno(error_file.get()), fileno(stderr)) == -1) {
            LOG_FATAL(jobType, sid, log_fp, RunnerError::DUP2_FAILED, "dup2 error file failed"_cptr);
            raise(SIGUSR1);
            return false;
        }
    }

    // set gid
    gid_t group_list[] = {config.gid};
    if (setgid(config.gid) == -1 || setgroups(sizeof(group_list) / sizeof(gid_t), group_list) == -1) {
        LOG_FATAL(jobType, sid, log_fp, RunnerError::SETUID_FAILED);
        raise(SIGUSR1);
        return false;
    }

    // set uid
    if (setuid(config.uid) == -1) {
        LOG_FATAL(jobType, sid, log_fp, RunnerError::SETUID_FAILED);
        raise(SIGUSR1);
        return false;
    }

    // load seccomp
    if (config.load_seccomp_rules() != RunnerError::SUCCESS) {
        LOG_FATAL(jobType, sid, log_fp, RunnerError::LOAD_SECCOMP_FAILED);
        raise(SIGUSR1);
        return false;
    }

    auto args = config.args.getArgs();
    auto env = config.env.getArgs();
    execve(config.exe_path, args.get(), env.get());
    /*
     * 以上三行不可以并作
     * execve(config.exe_path, config.args.getArgs().get(), config.env.getArgs().get());
     * 会取到野指针
     */

    LOG_FATAL(jobType, sid, log_fp, RunnerError::EXECVE_FAILED);
    raise(SIGUSR1);
    return false;
}


bool JudgeJob::push_back_failed_judge_job() noexcept
{
    return JudgeJob::insert_into_failed(redisConn, jobType, sid);
}


bool JudgeJob::insert_into_failed(const kerbal::redis::RedisContext & conn, int jobType, int sid) noexcept
{
    try {
        LOG_WARNING(jobType, sid, log_fp, "push back to judge failed list"_cptr);
        Result result;
        result.judge_result = UnitedJudgeResult::SYSTEM_ERROR;
        staticCommitJudgeResultToRedis(jobType, sid, conn, result);

        List<std::string> judge_failure_list(conn, "judge_failure_list");
        judge_failure_list.push_back("%d:%d"_fmt(jobType, sid).str());
        return true;
    } catch (const std::exception & e) {
        LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: "_cptr, e.what());
    } catch (...) {
        LOG_FATAL(jobType, sid, log_fp, "Failed to push back failed judge job. Error information: "_cptr,
                  UNKNOWN_EXCEPTION_WHAT);
    }
    return false;
}
