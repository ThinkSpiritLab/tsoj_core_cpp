compiler := g++
compiler_name := GCC C++ Compiler
compile_options := -std=c++11 -O2 -Wall -c -fmessage-length=0 -MMD -MP
obj_folder := ../../release/obj
linker := g++
linker_name := GCC C++ Linker
link_options := -lseccomp -lhiredis -pthread
master_bin_name := ../../release/job_giver


SRCS += \
./master.cpp

OBJS += \
$(obj_folder)/master.o

DEPS += \
$(obj_folder)/master.d

include_path += \
../shared_src

SHARED_OBJS += \
$(obj_folder)/logger.o \
$(obj_folder)/JobBase.o \
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
	
	
