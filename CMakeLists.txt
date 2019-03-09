cmake_minimum_required(VERSION 3.10)
project(Core)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")

include_directories(src/master)
include_directories(src/master/Management)
include_directories(src/master/RejudgeJob)
include_directories(src/master/UpdateJob)
include_directories(src/shared_src)
include_directories(src/slave)

#add_definitions(-DDEBUG)

set(SHARED_SRC
			src/shared_src/JobBase.cpp
			src/shared_src/logger.cpp
)

add_executable(ts_judge_master
			src/master/Management/ContestManagement.cpp
			src/master/Management/CourseManagement.cpp
			src/master/Management/ExerciseManagement.cpp
			src/master/RejudgeJob/ContestRejudgeJob.cpp
			src/master/RejudgeJob/CourseRejudgeJob.cpp
			src/master/RejudgeJob/ExerciseRejudgeJob.cpp
			src/master/RejudgeJob/ExerciseRejudgeJobBase.cpp
			src/master/RejudgeJob/RejudgeJobBase.cpp
			src/master/UpdateJob/ContestUpdateJob.cpp
			src/master/UpdateJob/CourseUpdateJob.cpp
			src/master/UpdateJob/ExerciseUpdateJob.cpp
			src/master/UpdateJob/ExerciseUpdateJobBase.cpp
			src/master/UpdateJob/UpdateJobBase.cpp
			src/master/master_settings.cpp
			src/master/master.cpp
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

