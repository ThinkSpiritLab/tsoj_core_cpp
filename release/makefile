
src := ../src

all: slave master
	@echo ' '

.PHONY: slave
slave:
	+@cd $(src)/slave && make slave configuration=release

.PHONY: master
master:
	+@cd $(src)/master && make master configuration=release

.PHONY: clean_shared
clean_shared:
	-@cd $(src)/shared_src && make clean configuration=release

.PHONY: clean_slave
clean_slave:
	-@cd $(src)/slave && make clean configuration=release

.PHONY: clean_master
clean_master:
	-@cd $(src)/master && make clean configuration=release

.PHONY: clean
clean: clean_shared clean_slave clean_master
	-$(RM) obj/ -rf
	@echo ' '
