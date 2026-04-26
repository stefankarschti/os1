#ifndef OS1_KERNEL_SYSCALL_PROCESS_H
#define OS1_KERNEL_SYSCALL_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "pageframe.h"

using ConsoleWriteBytes = void (*)(const char *data, size_t length);
using Cr3Write = void (*)(uint64_t cr3);

struct ProcessSyscallContext
{
	PageFrameContainer *frames = nullptr;
	uint64_t kernel_root_cr3 = 0;
	ConsoleWriteBytes write_console_bytes = nullptr;
	Cr3Write write_cr3 = nullptr;
};

long SysWrite(const ProcessSyscallContext &context, int fd, uint64_t user_buffer, size_t length);
long SysSpawn(const ProcessSyscallContext &context, uint64_t user_path);
long SysExec(const ProcessSyscallContext &context, uint64_t user_path);

#endif
