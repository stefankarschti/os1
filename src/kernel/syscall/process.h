// Process-oriented syscall bodies for write, spawn, and exec. The dispatch
// layer passes dependencies explicitly to keep syscall code testable and away
// from global kernel bring-up state.
#ifndef OS1_KERNEL_SYSCALL_PROCESS_H
#define OS1_KERNEL_SYSCALL_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.h"

using ConsoleWriteBytes = void (*)(const char *data, size_t length);
using Cr3Write = void (*)(uint64_t cr3);

struct ProcessSyscallContext
{
	PageFrameContainer *frames = nullptr;
	uint64_t kernel_root_cr3 = 0;
	ConsoleWriteBytes write_console_bytes = nullptr;
	Cr3Write write_cr3 = nullptr;
};

// Copy user bytes to the console stream for supported file descriptors.
long SysWrite(const ProcessSyscallContext &context, int fd, uint64_t user_buffer, size_t length);
// Load an initrd program into a new child process and make it runnable.
long SysSpawn(const ProcessSyscallContext &context, uint64_t user_path);
// Replace the current process image with another initrd program.
long SysExec(const ProcessSyscallContext &context, uint64_t user_path);

#endif // OS1_KERNEL_SYSCALL_PROCESS_H
