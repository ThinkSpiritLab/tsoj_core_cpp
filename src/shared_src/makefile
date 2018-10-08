compiler := g++
compiler_name := GCC C++ Compiler

ifeq ($(configuration), release)
	obj_folder := ../../release/obj
	compile_options := -std=c++11 -O2 -Wall -c -fmessage-length=0 -MMD -MP
else ifeq ($(configuration), debug)
	obj_folder := ../../debug/obj
	compile_options := -std=c++11 -DTRACK_RESOURCE_FREE=false -DDEBUG -O2 -g3 -Wall -c -fmessage-length=0 -MMD -MP
else
$(error undefined configuration)
endif

SRCS += \
./JobBase.cpp \
./load_helper.cpp \
./logger.cpp \
./Result.cpp \
./united_resource.cpp

OBJS += \
$(obj_folder)/JobBase.o \
$(obj_folder)/load_helper.o \
$(obj_folder)/logger.o \
$(obj_folder)/Result.o \
$(obj_folder)/united_resource.o

DEPS += \
$(obj_folder)/JobBase.d \
$(obj_folder)/load_helper.d \
$(obj_folder)/logger.d \
$(obj_folder)/Result.d \
$(obj_folder)/united_resource.d

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
