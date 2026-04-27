// Kernel-private syscall number list. User-space wrappers should stay in libc;
// this header defines the register ABI values handled by syscall/dispatch.cpp.
#ifndef OS1_KERNEL_SYSCALL_ABI_H
#define OS1_KERNEL_SYSCALL_ABI_H

#include <stdint.h>

enum : uint64_t
{
	SYS_write = 1,
	SYS_exit = 2,
	SYS_yield = 3,
	SYS_getpid = 4,
	SYS_read = 5,
	SYS_observe = 6,
	SYS_spawn = 7,
	SYS_waitpid = 8,
	SYS_exec = 9,
};

#endif // OS1_KERNEL_SYSCALL_ABI_H
