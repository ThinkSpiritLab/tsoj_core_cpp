
src := ../src

all: slave master
	@echo ' '

.PHONY: slave
slave:
	+@cd $(src)/slave && make slave configuration=release \
									compiler=g++ \
									compiler_name="GCC C++ Compiler" \
									compile_options="-std=c++17 -O2 -Wall -c -DPROFILE -fmessage-length=0 -MMD -MP" \
									linker=g++ \
									linker_name="GCC C++ Linker" \
									link_options="-lseccomp -lhiredis -pthread" \
									obj_folder=../../profile/obj \
									slave_bin_name=../../profile/ts_judger_slave


.PHONY: master
master:
	+@cd $(src)/master && make master configuration=release \
									compiler=g++ \
									compiler_name="GCC C++ Compiler" \
									compile_options="-std=c++17 -O2 -Wall -c -DPROFILE -fmessage-length=0 -MMD -MP" \
									linker=g++ \
									linker_name="GCC C++ Linker" \
									link_options="-lseccomp -lhiredis -lmysqlpp -lmysqlclient -pthread" \
									obj_folder=../../profile/obj \
									master_bin_name=../../profile/ts_judger_master
	

.PHONY: clean_shared
clean_shared:
	-@cd $(src)/shared_src && make clean configuration=release \
										obj_folder=../../profile/obj

.PHONY: clean_slave
clean_slave:
	-@cd $(src)/slave && make clean configuration=release \
										obj_folder=../../profile/obj \
										slave_bin_name=../../profile/ts_judger_slave

.PHONY: clean_master
clean_master:
	-@cd $(src)/master && make clean configuration=release \
										obj_folder=../../profile/obj \
										master_bin_name=../../profile/ts_judger_master

.PHONY: clean
clean: clean_shared clean_slave clean_master
	-$(RM) obj/ -rf
	@echo ' '
