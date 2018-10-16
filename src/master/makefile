compiler := g++
compiler_name := GCC C++ Compiler

ifeq ($(configuration), release)
	obj_folder := ../../release/obj
	compile_options := -std=c++11 -O2 -Wall -c -fmessage-length=0 -MMD -MP
	master_bin_name := ../../release/master
else ifeq ($(configuration), debug)
	obj_folder := ../../debug/obj
	compile_options := -std=c++11 -DTRACK_RESOURCE_FREE=false -DDEBUG -O2 -g3 -Wall -c -fmessage-length=0 -MMD -MP
	master_bin_name := ../../debug/master
else
$(error undefined configuration)
endif

linker := g++
linker_name := GCC C++ Linker
link_options := -lseccomp -lhiredis -lmysqlpp -pthread


SRCS += \
./ContestUpdateJob.cpp \
./CourseUpdateJob.cpp \
./ExerciseUpdateJob.cpp \
./ExerciseUpdateJobBase.cpp \
./master.cpp \
./ContestScoreboardUpdate.cpp \
./UpdateJobBase.cpp


OBJS += \
$(obj_folder)/ContestUpdateJob.o \
$(obj_folder)/CourseUpdateJob.o \
$(obj_folder)/ExerciseUpdateJob.o \
$(obj_folder)/ExerciseUpdateJobBase.o \
$(obj_folder)/master.o \
$(obj_folder)/ContestScoreboardUpdate.o \
$(obj_folder)/UpdateJobBase.o

DEPS += \
$(obj_folder)/ContestUpdateJob.d \
$(obj_folder)/CourseUpdateJob.d \
$(obj_folder)/ExerciseUpdateJob.d \
$(obj_folder)/ExerciseUpdateJobBase.d \
$(obj_folder)/master.d \
$(obj_folder)/ContestScoreboardUpdate.d \
$(obj_folder)/UpdateJobBase.d

include_path += \
../shared_src

SHARED_OBJS += \
$(obj_folder)/JobBase.o \
$(obj_folder)/load_helper.o \
$(obj_folder)/logger.o \
$(obj_folder)/Result.o \
$(obj_folder)/united_resource.o

BLUE_TEXT_BEGIN := \033[1m\033[36m
YELLOW_TEXT_BEGIN := \033[1m\033[33m
DEFAULT_TEXT_BEGIN := \033[0m

$(obj_folder):
	@mkdir $(obj_folder) -p

$(obj_folder)/%.o: %.cpp
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo '$(YELLOW_TEXT_BEGIN)Building file: $<' && echo 'Invoking: $(compiler_name)$(DEFAULT_TEXT_BEGIN)'
	$(compiler) $(compile_options) -I"$(include_path)" -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo -e '$(BLUE_TEXT_BEGIN)Finished building: $<$(DEFAULT_TEXT_BEGIN)\n'


master: shared $(obj_folder) $(OBJS)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo '$(YELLOW_TEXT_BEGIN)Building target: $@' && echo 'Invoking: $(linker_name)$(DEFAULT_TEXT_BEGIN)'
	$(linker) -o $(master_bin_name) $(OBJS) $(SHARED_OBJS) $(link_options)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo 'Finished building target: $@$(DEFAULT_TEXT_BEGIN)\n'

shared:
	+@cd ../shared_src/ && make shared

clean:
	-$(RM) $(OBJS) $(DEPS) $(master_bin_name)
	-@echo ' '
	
	
