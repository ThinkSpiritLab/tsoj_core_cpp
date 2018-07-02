#include <stdio.h>
#include <seccomp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "seccomp_rules.hpp"

RunnerError c_cpp_seccomp_rules(const Config & _config)
{
	int syscalls_whitelist[] = {
		SCMP_SYS(read), SCMP_SYS(fstat), SCMP_SYS(mmap), SCMP_SYS(mprotect), SCMP_SYS(munmap), SCMP_SYS(uname), SCMP_SYS(arch_prctl), SCMP_SYS(brk), SCMP_SYS(access), SCMP_SYS(exit_group), SCMP_SYS(
				close), SCMP_SYS(readlink), SCMP_SYS(sysinfo), SCMP_SYS(write), SCMP_SYS(writev), SCMP_SYS(lseek) };

	int syscalls_whitelist_length = sizeof(syscalls_whitelist) / sizeof(int);
	scmp_filter_ctx ctx = NULL;
	// load seccomp rules
	ctx = seccomp_init(SCMP_ACT_KILL);
	if (!ctx) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	for (int i = 0; i < syscalls_whitelist_length; i++) {
		if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls_whitelist[i], 0) != 0) {
			return RunnerError::LOAD_SECCOMP_FAILED;
		}
	}
	// add extra rule for execve
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t )(_config.exe_path))) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// do not allow "w" and "rw"
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	if (seccomp_load(ctx) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	seccomp_release(ctx);
	return RunnerError::SUCCESS;
}

RunnerError general_seccomp_rules(const Config & _config)
{
	int syscalls_blacklist[] = { SCMP_SYS(socket), SCMP_SYS(clone), SCMP_SYS(fork), SCMP_SYS(vfork), SCMP_SYS(kill) };
	int syscalls_blacklist_length = sizeof(syscalls_blacklist) / sizeof(int);
	scmp_filter_ctx ctx = NULL;
	// load seccomp rules
	ctx = seccomp_init(SCMP_ACT_ALLOW);
	if (!ctx) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	for (int i = 0; i < syscalls_blacklist_length; i++) {
		if (seccomp_rule_add(ctx, SCMP_ACT_KILL, syscalls_blacklist[i], 0) != 0) {
			return RunnerError::LOAD_SECCOMP_FAILED;
		}
	}
	// add extra rule for execve
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_NE, (scmp_datum_t )(_config.exe_path))) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	// do not allow "w" and "rw"
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	if (seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	if (seccomp_load(ctx) != 0) {
		return RunnerError::LOAD_SECCOMP_FAILED;
	}
	seccomp_release(ctx);
	return RunnerError::SUCCESS;
}
