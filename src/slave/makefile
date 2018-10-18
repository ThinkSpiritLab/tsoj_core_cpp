compiler := g++
compiler_name := GCC C++ Compiler

ifeq ($(configuration), release)
	obj_folder := ../../release/obj
	compile_options := -std=c++11 -O2 -Wall -c -fmessage-length=0 -MMD -MP
	slave_bin_name := ../../release/ts_judger_slave
else ifeq ($(configuration), debug)
	obj_folder := ../../debug/obj
	compile_options := -std=c++11 -DTRACK_RESOURCE_FREE=false -DDEBUG -O2 -g3 -Wall -c -fmessage-length=0 -MMD -MP
	slave_bin_name := ../../debug/ts_judger_slave
else
$(error undefined configuration)
endif

linker := g++
linker_name := GCC C++ Linker
link_options := -lseccomp -lhiredis -pthread


SRCS += \
./compare.cpp \
./Config.cpp \
./ExecuteArgs.cpp \
./JudgeJob.cpp \
./slave.cpp

OBJS += \
$(obj_folder)/compare.o \
$(obj_folder)/Config.o \
$(obj_folder)/ExecuteArgs.o \
$(obj_folder)/JudgeJob.o \
$(obj_folder)/slave.o

DEPS += \
$(obj_folder)/compare.d \
$(obj_folder)/Config.d \
$(obj_folder)/ExecuteArgs.d \
$(obj_folder)/JudgeJob.d \
$(obj_folder)/slave.d

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


slave: shared $(obj_folder) $(OBJS)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo '$(YELLOW_TEXT_BEGIN)Building target: $@' && echo 'Invoking: $(linker_name)$(DEFAULT_TEXT_BEGIN)'
	$(linker) -o $(slave_bin_name) $(OBJS) $(SHARED_OBJS) $(link_options)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo 'Finished building target: $@$(DEFAULT_TEXT_BEGIN)\n'

shared:
	+@cd ../shared_src/ && make shared

clean:
	-$(RM) $(OBJS) $(DEPS) $(slave_bin_name)
	-@echo ' '
	

