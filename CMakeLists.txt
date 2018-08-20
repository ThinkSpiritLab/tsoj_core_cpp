cmake_minimum_required(VERSION 3.12)
project(Core)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/master)
include_directories(src/shared_src)
include_directories(src/slave)

add_executable(Core
        release/makefile
        src/master/job_giver_global_shared_variable.hpp
        src/master/master.cpp
        src/master/rejudge.cpp
        src/master/UpdatorJob.cpp
        src/master/UpdatorJob.hpp
        src/shared_src/boost_format_suffix.hpp
        src/shared_src/cptr.hpp
        src/shared_src/JobBase.cpp
        src/shared_src/JobBase.hpp
        src/shared_src/load_helper.cpp
        src/shared_src/load_helper.hpp
        src/shared_src/logger.cpp
        src/shared_src/logger.hpp
        src/shared_src/process.hpp
        src/shared_src/Result.cpp
        src/shared_src/Result.hpp
        src/shared_src/united_resource.cpp
        src/shared_src/united_resource.hpp
        src/slave/compare.cpp
        src/slave/compare.hpp
        src/slave/Config.cpp
        src/slave/Config.hpp
        src/slave/ExecuteArgs.cpp
        src/slave/ExecuteArgs.hpp
        src/slave/global_shared_variable.hpp
        src/slave/JudgeJob.cpp
        src/slave/JudgeJob.hpp
        src/slave/slave.cpp
        main.cpp)
