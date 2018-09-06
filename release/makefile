include configuration.mk
src := ../src

all: slave master
	@echo ' '

.PHONY: slave
slave:
	+@cd $(src)/slave && make slave configuration=$(configuration)

.PHONY: master
master:
	+@cd $(src)/master && make master configuration=$(configuration)

.PHONY: clean_shared
clean_shared:
	-@cd $(src)/shared_src && make clean configuration=$(configuration)

.PHONY: clean_slave
clean_slave:
	-@cd $(src)/slave && make clean configuration=$(configuration)

.PHONY: clean_master
clean_master:
	-@cd $(src)/master && make clean configuration=$(configuration)

.PHONY: clean
clean: clean_shared clean_slave clean_master
	-$(RM) obj/ -rf
	@echo ' '
