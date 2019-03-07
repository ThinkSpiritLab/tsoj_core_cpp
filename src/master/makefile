
include_path_options += \
-I"./" \
-I"../shared_src/" \
-I"./Management/" \
-I"./RejudgeJob/" \
-I"./UpdateJob/"

-include ./Management/subdir.mk
-include ./RejudgeJob/subdir.mk
-include ./UpdateJob/subdir.mk
-include ./subdir.mk

SHARED_OBJS += \
$(obj_folder)/JobBase.o \
$(obj_folder)/load_helper.o \
$(obj_folder)/logger.o

BLUE_TEXT_BEGIN := \033[1m\033[36m
YELLOW_TEXT_BEGIN := \033[1m\033[33m
DEFAULT_TEXT_BEGIN := \033[0m


$(obj_folder)/%.o: %.cpp
	@mkdir $(obj_folder)/$(shell dirname $<) -p
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo '$(YELLOW_TEXT_BEGIN)Building file: $<' && echo 'Invoking: $(compiler_name)$(DEFAULT_TEXT_BEGIN)'
	$(compiler) $(compile_options) $(include_path_options) -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo -e '$(BLUE_TEXT_BEGIN)Finished building: $<$(DEFAULT_TEXT_BEGIN)\n'


master: shared $(OBJS)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo '$(YELLOW_TEXT_BEGIN)Building target: $@' && echo 'Invoking: $(linker_name)$(DEFAULT_TEXT_BEGIN)'
	$(linker) -o $(master_bin_name) $(OBJS) $(SHARED_OBJS) $(link_options)
	@echo '$(BLUE_TEXT_BEGIN)\c' && date +"[%H:%M:%S]" && echo 'Finished building target: $@$(DEFAULT_TEXT_BEGIN)\n'

shared:
	+@cd ../shared_src/ && make shared

clean:
	-$(RM) $(obj_folder) -rf
	-$(RM) $(master_bin_name)
	-@echo ' '
	
	
