#ifndef OS1_KERNEL_MM_USER_COPY_H
#define OS1_KERNEL_MM_USER_COPY_H

#include <stddef.h>
#include <stdint.h>

#include "pageframe.h"
#include "task.h"
#include "virtualmemory.h"

// Keep all syscall copy validation in one place. The kernel still maps low
// supervisor identity ranges into every address space, so this layer is the
// audited boundary that prevents syscalls from turning translation into a
// kernel-memory copy gadget.

bool CopyIntoAddressSpace(VirtualMemory &vm, uint64_t virtual_address, const uint8_t *source, uint64_t length);
bool CopyToUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, const void *source, size_t length);
bool CopyFromUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, void *destination, size_t length);
bool CopyUserString(PageFrameContainer &frames,
		const Thread *thread,
		uint64_t user_pointer,
		char *destination,
		size_t destination_size);

#endif
