toolchain := g++
obj_folder := ./src/
cpp_std := -std=c++11
judge_server_bin_name := "judge_server"

all: judge_server

OBJ_PATHS += \
./src/ \
./src/rules/

CPP_SRCS += \
./src/JobInfo.cpp \
./src/main.cpp \
./src/Config.cpp \
./src/united_resource.cpp \
./src/ExecuteArgs.cpp \
./src/compare.cpp \
./src/Result.cpp \
./src/logger.cpp \
./src/rules/seccomp_rules.cpp 

OBJS += \
./src/JobInfo.o \
./src/main.o \
./src/Config.o \
./src/united_resource.o \
./src/ExecuteArgs.o \
./src/compare.o \
./src/Result.o \
./src/logger.o \
./src/rules/seccomp_rules.o 

CPP_DEPS += \
./src/JobInfo.d \
./src/main.d \
./src/Config.d \
./src/united_resource.d \
./src/ExecuteArgs.d \
./src/compare.d \
./src/Result.d \
./src/logger.d \
./src/rules/seccomp_rules.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo '\033[1m\033[36mCompile start time: \c' && date +"%H:%M:%S" && echo '\033[1m\033[33mBuilding file: $<\nInvoking: GCC C++ Compiler\033[0m'
	$(toolchain) $(cpp_std) -O2 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# Tool invocations
judge_server: make_start $(OBJ_PATHS) $(OBJS)
	@echo -e '\033[1m\033[33m\c'
	@echo -e 'Building target: $@'
	@echo -e 'Invoking: GCC C++ Linker\c'
	@echo -e '\033[0m'
	$(toolchain) -pthread -o $(judge_server_bin_name) $(OBJS) -lseccomp -lhiredis
	@echo 'Finished building target: $@'

	@echo -e '\033[1m\033[36m\c'
	@echo -e 'End time: \c'
	@date +"%H:%M:%S"
	@echo ' '

make_start:
	@echo '\033[1m\033[36mMake Start time: ' && date +"%H:%M:%S" && echo '\033[0m'

%/:
ifeq ('$@', $(wildcard $@))
	@echo '$@ directory exists'
else
	@echo 'create directory $@'
	@mkdir $@
endif




RUNNING_PATHS += \
/etc/ts_judger/ \
/dev/shm/ \
/dev/shm/judge_space/ \
/dev/shm/judge_space/run/ \
/dev/shm/judge_space/update/ \
/var/log/ \
/var/log/ts_judger/



install: $(RUNNING_PATHS)

	@echo "copying judge_server.conf and updater.conf to /etc/ts_judger/ ..."

	@-cp ../judge_server.conf /etc/ts_judger/judge_server.conf
	@-cp ../updater.conf /etc/ts_judger/updater.conf

# 新建一个用户ts_judger，专门用来评测
# 1666是一个任意的合法uid，需保证和ts_judger.conf中的配置一致
	@echo "adding a new user ts_judger ..."
	@-useradd -u 1666  -d /home/ts_judger  -m ts_judger

# java权限文件
	@-cp ../java.policy /dev/shm/judge_space
	@-chmod 755 /dev/shm/judge_space/java.policy
	@-chown ts_judger:ts_judger /dev/shm/judge_space/java.policy

	
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'Install success \c'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) $(judge_server_bin_name)
	-$(RM) $(OBJ_PATHS) -rf
	-@echo ' '
