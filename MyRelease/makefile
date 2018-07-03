
obj_folder := ./src/
cpp_std := -std=c++11
	
all: 
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'Make Start time: \c'
	@date +"%H:%M:%S"
	@echo -e '\033[0m'
	
ifeq ($(obj_folder), $(wildcard $(obj_folder)))
	@echo '$(obj_folder) exists'
else
	@mkdir $(obj_folder)
endif
	
	@make distributed_judge_server -j3
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'End time: \c'
	@date +"%H:%M:%S"
	@echo ' '

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
./src/seccomp_rules.o 

CPP_DEPS += \
./src/JobInfo.d \
./src/main.d \
./src/Config.d \
./src/united_resource.d \
./src/ExecuteArgs.d \
./src/compare.d \
./src/Result.d \
./src/logger.d \
./src/seccomp_rules.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'Compile start time: \c'
	@date +"%H:%M:%S"
	@echo -e '\033[1m\033[33m\c'
	@echo -e 'Building file: $<'
	@echo -e 'Invoking: GCC C++ Compiler\c'
	@echo -e '\033[0m'
	g++ $(cpp_std) -I"/home/peter/git/Kerbal/Kerbal/include" -O2 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
	
# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/rules/%.cpp
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'Compile start time: \c'
	@date +"%H:%M:%S"
	@echo -e '\033[1m\033[33m\c'
	@echo -e 'Building file: $<'
	@echo -e 'Invoking: GCC C++ Compiler\c'
	@echo -e '\033[0m'
	g++ $(cpp_std) -I"/home/peter/git/Kerbal/Kerbal/include" -O2 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Tool invocations
distributed_judge_server: $(OBJS) $(USER_OBJS)
	@echo -e '\033[1m\033[33m\c'
	@echo -e 'Building target: $@'
	@echo -e 'Invoking: GCC C++ Linker\c'
	@echo -e '\033[0m'
	g++ -L"/home/peter/git/Kerbal/Kerbal/Release" -pthread -o "distributed_judge_server" $(OBJS) $(USER_OBJS) -lseccomp -lhiredis
	@echo 'Finished building target: $@'

# Other Targets
clean:
	-$(RM) $(CC_DEPS)$(C++_DEPS)$(EXECUTABLES)$(C_UPPER_DEPS)$(CXX_DEPS)$(OBJS)$(CPP_DEPS)$(C_DEPS) distributed_judge_server
	-@echo ' '
