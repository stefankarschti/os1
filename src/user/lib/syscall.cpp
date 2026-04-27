#include <stdint.h>

extern "C" long os1_syscall0(uint64_t number)
{
	long result = 0;
	asm volatile("int $0x80"
		: "=a"(result)
		: "a"(number)
		: "memory", "cc");
	return result;
}

extern "C" long os1_syscall1(uint64_t number, uint64_t arg0)
{
	long result = 0;
	asm volatile("int $0x80"
		: "=a"(result)
		: "a"(number), "D"(arg0)
		: "memory", "cc");
	return result;
}

extern "C" long os1_syscall2(uint64_t number, uint64_t arg0, uint64_t arg1)
{
	long result = 0;
	asm volatile("int $0x80"
		: "=a"(result)
		: "a"(number), "D"(arg0), "S"(arg1)
		: "memory", "cc");
	return result;
}

extern "C" long os1_syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	long result = 0;
	asm volatile("int $0x80"
		: "=a"(result)
		: "a"(number), "D"(arg0), "S"(arg1), "d"(arg2)
		: "memory", "cc");
	return result;
}
