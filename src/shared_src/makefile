
SRCS += \
./JobBase.cpp \
./logger.cpp

OBJS += \
$(obj_folder)/JobBase.o \
$(obj_folder)/logger.o

BLUE_TEXT_BEGIN := \033[1m\033[36m
YELLOW_TEXT_BEGIN := \033[1m\033[33m
DEFAULT_TEXT_BEGIN := \033[0m

shared: $(obj_folder) $(OBJS)

.PHONY: $(obj_folder)
$(obj_folder):
	@mkdir $(obj_folder) -p

$(obj_folder)/%.o: %.cpp
	@echo '$(BLUE_TEXT_BEGIN)\c'; \
	date +"[%H:%M:%S]"; \
	echo '$(YELLOW_TEXT_BEGIN)Building file: $<'; \
	echo 'Invoking: $(compiler_name)$(DEFAULT_TEXT_BEGIN)'
	$(compiler) $(compile_options) -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo -e '$(BLUE_TEXT_BEGIN)Finished building: $<$(DEFAULT_TEXT_BEGIN)\n'


clean:
	-$(RM) $(OBJS) $(DEPS)
	-@echo ' '
