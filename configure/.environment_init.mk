
.PHONY: environment_init
environment_init:
	+mkdir -p /var/log/ts_judger

	+mkdir -p /etc/ts_judger
	-cp ./judge_server.conf /etc/ts_judger/judge_server.conf
	-cp ./updater.conf /etc/ts_judger/updater.conf

# 新建一个用户ts_judger，专门用来评测
# 1666是一个任意的合法uid，需保证和ts_judger.conf中的配置一致
	-useradd -u 1666  -d /home/ts_judger -m ts_judger

# java权限文件
	-cp ./java.policy /dev/shm/judge_space
	-chmod 755 /dev/shm/judge_space/java.policy
	-chown ts_judger:ts_judger /dev/shm/judge_space/java.policy

	
	@echo -e '\033[1m\033[36m\c'
	@echo -e 'Environment initial finished \c'
	@echo ' '
