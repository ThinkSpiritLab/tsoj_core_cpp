
CUR_DIR := $(shell pwd)
PROJECT_HOME := $(shell pwd)/project_home

-include ./var.mk

SLAVE_BIN_NAME := "ts_judger_slave"
MASTER_BIN_NAME := "ts_judger_master"

all: slave master
	@echo ' '

.PHONY: slave
slave:
	+@echo $(CUR_DIR)
	+@cd $(PROJECT_HOME)/src/slave && make slave configuration=$(CONFIGURATION) \
									compiler=$(COMPILER) \
									compiler_name=$(COMPILER_NAME) \
									compile_options=$(COMPILE_OPTION) \
									linker=$(LINKER) \
									linker_name=$(LINKER_NAME) \
									link_options=$(SLAVE_LINK_OPTION) \
									obj_folder=$(CUR_DIR)/obj \
									slave_bin_name=$(CUR_DIR)/$(SLAVE_BIN_NAME)


.PHONY: master
master:
	+@cd $(PROJECT_HOME)/src/master && make master configuration=$(CONFIGURATION) \
									compiler=$(COMPILER) \
									compiler_name=$(COMPILER_NAME) \
									compile_options=$(COMPILE_OPTION) \
									linker=$(LINKER) \
									linker_name=$(LINKER_NAME) \
									link_options=$(MASTER_LINK_OPTION) \
									obj_folder=$(CUR_DIR)/obj \
									master_bin_name=$(CUR_DIR)/$(MASTER_BIN_NAME)
	

.PHONY: clean_shared
clean_shared:
	-@cd $(PROJECT_HOME)/src/shared_src && make clean configuration=$(CONFIGURATION) \
										obj_folder=$(CUR_DIR)/obj

.PHONY: clean_slave
clean_slave:
	-@cd $(PROJECT_HOME)/src/slave && make clean configuration=$(CONFIGURATION) \
										obj_folder=$(CUR_DIR)/obj \
										slave_bin_name=$(CUR_DIR)/ts_judger_slave

.PHONY: clean_master
clean_master:
	-@cd $(PROJECT_HOME)/src/master && make clean configuration=$(CONFIGURATION) \
										obj_folder=$(CUR_DIR)/obj \
										master_bin_name=$(CUR_DIR)/ts_judger_master

.PHONY: clean
clean: clean_shared clean_slave clean_master
	-$(RM) obj/ -rf
	@echo ' '
