#ifndef OS1_KERNEL_PROC_USER_PROGRAM_H
#define OS1_KERNEL_PROC_USER_PROGRAM_H

#include <stdint.h>

#include "pageframe.h"
#include "task.h"

// User-program loading owns the current initrd-backed ELF path and the
// initial ring-3 thread frame shape. Syscalls and KernelMain pass dependencies
// in explicitly so this code stays separate from global boot orchestration.

bool DestroyUserAddressSpace(PageFrameContainer &frames, uint64_t cr3);
bool LoadUserProgramImage(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const char *path,
		uint64_t &user_cr3,
		uint64_t &entry,
		uint64_t &user_rsp);
void PrepareUserThreadEntry(Thread *thread, uint64_t entry, uint64_t user_rsp);
Thread *LoadUserProgram(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const char *path,
		Process *parent = nullptr);

#endif
