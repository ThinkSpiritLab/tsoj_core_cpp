
src := ../src

all: slave master
	@echo ' '

.PHONY: slave
slave:
	+@cd $(src)/slave && make slave configuration=debug \
									compiler=g++ \
									compiler_name="GCC C++ Compiler" \
									compile_options="-std=c++11 -DTRACK_RESOURCE_FREE=false -DDEBUG -O2 -g3 -Wall -c -fmessage-length=0 -MMD -MP" \
									linker=g++ \
									linker_name="GCC C++ Linker" \
									link_options="-lseccomp -lhiredis -lmysqlpp -pthread" \
									obj_folder=../../debug/obj \
									slave_bin_name=../../debug/ts_judger_slave


.PHONY: master
master:
	+@cd $(src)/master && make master configuration=debug \
									compiler=g++ \
									compiler_name="GCC C++ Compiler" \
									compile_options="-std=c++11 -DTRACK_RESOURCE_FREE=false -DDEBUG -O2 -g3 -Wall -c -fmessage-length=0 -MMD -MP" \
									linker=g++ \
									linker_name="GCC C++ Linker" \
									link_options="-lhiredis -lmysqlpp -pthread" \
									obj_folder=../../debug/obj \
									master_bin_name=../../debug/ts_judger_master

.PHONY: clean_shared
clean_shared:
	-@cd $(src)/shared_src && make clean configuration=debug \
										obj_folder=../../debug/obj

.PHONY: clean_slave
clean_slave:
	-@cd $(src)/slave && make clean configuration=debug \
										obj_folder=../../debug/obj \
										slave_bin_name=../../debug/ts_judger_slave

.PHONY: clean_master
clean_master:
	-@cd $(src)/master && make clean configuration=debug \
										obj_folder=../../debug/obj \
										master_bin_name=../../debug/ts_judger_master

.PHONY: clean
clean: clean_shared clean_slave clean_master
	-$(RM) obj/ -rf
	@echo ' '
