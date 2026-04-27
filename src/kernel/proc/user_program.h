#ifndef OS1_KERNEL_PROC_USER_PROGRAM_H
#define OS1_KERNEL_PROC_USER_PROGRAM_H

#include <stdint.h>

#include "mm/page_frame.h"
#include "proc/thread.h"

// User-program loading owns the current initrd-backed ELF path and the initial
// ring-3 thread frame shape. Syscalls and KernelMain pass dependencies in
// explicitly so this code stays separate from global boot orchestration.

// Destroy the user slot and root page backing a process address space.
bool DestroyUserAddressSpace(PageFrameContainer &frames, uint64_t cr3);
// Load an ELF image from the initrd into a fresh user address space.
bool LoadUserProgramImage(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const char *path,
		uint64_t &user_cr3,
		uint64_t &entry,
		uint64_t &user_rsp);
// Rewrite an existing thread's trap frame so it enters a loaded user image.
void PrepareUserThreadEntry(Thread *thread, uint64_t entry, uint64_t user_rsp);
// Load a user program from the initrd and create the first runnable thread.
Thread *LoadUserProgram(PageFrameContainer &frames,
		uint64_t kernel_root_cr3,
		const char *path,
		Process *parent = nullptr);

#endif
