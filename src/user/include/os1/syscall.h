#ifndef _OS1_USER_SYSCALL_H_
#define _OS1_USER_SYSCALL_H_

#include <stddef.h>
#include <stdint.h>

enum {
	SYS_write = 1,
	SYS_exit = 2,
	SYS_yield = 3,
	SYS_getpid = 4,
	SYS_read = 5,
	SYS_observe = 6
};

#ifdef __cplusplus
extern "C" {
#endif

long os1_syscall0(uint64_t number);
long os1_syscall1(uint64_t number, uint64_t arg0);
long os1_syscall2(uint64_t number, uint64_t arg0, uint64_t arg1);
long os1_syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2);

#ifdef __cplusplus
}
#endif

static inline long os1_write(int fd, const void *buffer, size_t length)
{
	return os1_syscall3(SYS_write, (uint64_t)fd, (uint64_t)buffer, (uint64_t)length);
}

static inline long os1_read(int fd, void *buffer, size_t length)
{
	return os1_syscall3(SYS_read, (uint64_t)fd, (uint64_t)buffer, (uint64_t)length);
}

static inline long os1_observe(uint64_t kind, void *buffer, size_t length)
{
	return os1_syscall3(SYS_observe, kind, (uint64_t)buffer, (uint64_t)length);
}

static inline void os1_exit(int status)
{
	os1_syscall1(SYS_exit, (uint64_t)status);
	for(;;)
	{
		asm volatile("hlt");
	}
}

static inline void os1_yield(void)
{
	os1_syscall0(SYS_yield);
}

static inline long os1_getpid(void)
{
	return os1_syscall0(SYS_getpid);
}

#endif
