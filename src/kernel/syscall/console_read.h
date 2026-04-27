// Blocking console-read syscall helpers. These functions complete immediately
// when a line is queued or put the current thread to sleep until input arrives.
#ifndef OS1_KERNEL_SYSCALL_CONSOLE_READ_H
#define OS1_KERNEL_SYSCALL_CONSOLE_READ_H

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.h"
#include "proc/thread.h"

// Try to satisfy a read syscall into user memory, possibly blocking `thread`.
bool TryCompleteConsoleRead(PageFrameContainer &frames,
		Thread *thread,
		uint64_t user_buffer,
		size_t length,
		long &result);
// Wake console-read waiters after input arrives.
void WakeConsoleReaders(PageFrameContainer &frames);

#endif // OS1_KERNEL_SYSCALL_CONSOLE_READ_H
