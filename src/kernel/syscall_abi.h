#ifndef _SYSCALL_ABI_H_
#define _SYSCALL_ABI_H_

#include <stdint.h>

enum : uint64_t
{
	SYS_write = 1,
	SYS_exit = 2,
	SYS_yield = 3,
	SYS_getpid = 4,
};

#endif
