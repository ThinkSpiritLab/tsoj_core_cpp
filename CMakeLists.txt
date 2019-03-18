cmake_minimum_required(VERSION 3.10)
project(Core)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

include_directories(src/master)
include_directories(src/master/Contest)
include_directories(src/master/Course)
include_directories(src/master/Exercise)
include_directories(src/master/Management)
include_directories(src/shared_src)
include_directories(src/slave)

#add_definitions(-DDEBUG)

set(SHARED_SRC
			src/shared_src/JobBase.cpp
			src/shared_src/logger.cpp
)

add_executable(ts_judge_master
			src/master/Contest/ContestUpdateJob.cpp
			src/master/Course/CourseUpdateJob.cpp
			src/master/Exercise/ExerciseUpdateJob.cpp
			src/master/Management/ContestManagement.cpp
			src/master/Management/CourseManagement.cpp
			src/master/Management/ExerciseManagement.cpp
			src/master/BasicUpdateJobInterface.cpp
			src/master/master.cpp
			src/master/master_settings.cpp
			src/master/RejudgeJobInterface.cpp
			src/master/UpdateJobInterface.cpp
			${SHARED_SRC}
)

target_link_libraries(ts_judge_master
			hiredis
			mysqlpp
			mysqlclient
			boost_filesystem
			pthread
)

add_executable(ts_judge_slave
			src/slave/compare.cpp
			src/slave/ExecuteArgs.cpp
			src/slave/JudgeJob.cpp
			src/slave/ProtectedProcess.cpp
			src/slave/SingleTestJudgeJob.cpp
			src/slave/slave.cpp
			${SHARED_SRC}
)

