#ifndef OS1_KERNEL_MM_USER_COPY_H
#define OS1_KERNEL_MM_USER_COPY_H

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.h"
#include "proc/thread.h"
#include "mm/virtual_memory.h"

// Keep all syscall copy validation in one place. The kernel still maps low
// supervisor identity ranges into every address space, so this layer is the
// audited boundary that prevents syscalls from turning translation into a
// kernel-memory copy gadget.

// Copy bytes into an arbitrary mapped address space using page-table translation.
bool CopyIntoAddressSpace(VirtualMemory &vm, uint64_t virtual_address, const uint8_t *source, uint64_t length);
// Copy kernel bytes into the current thread's user address space after validation.
bool CopyToUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, const void *source, size_t length);
// Copy user bytes into a kernel buffer after validation.
bool CopyFromUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, void *destination, size_t length);
// Copy a nul-terminated user string into a bounded kernel buffer.
bool CopyUserString(PageFrameContainer &frames,
		const Thread *thread,
		uint64_t user_pointer,
		char *destination,
		size_t destination_size);

#endif // OS1_KERNEL_MM_USER_COPY_H
