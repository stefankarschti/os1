#ifndef OS1_KERNEL_SYSCALL_CONSOLE_READ_H
#define OS1_KERNEL_SYSCALL_CONSOLE_READ_H

#include <stddef.h>
#include <stdint.h>

#include "pageframe.h"
#include "task.h"

bool TryCompleteConsoleRead(PageFrameContainer &frames,
		Thread *thread,
		uint64_t user_buffer,
		size_t length,
		long &result);
void WakeConsoleReaders(PageFrameContainer &frames);

#endif
