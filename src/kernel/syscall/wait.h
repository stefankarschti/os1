#ifndef OS1_KERNEL_SYSCALL_WAIT_H
#define OS1_KERNEL_SYSCALL_WAIT_H

#include <stdint.h>

#include "pageframe.h"
#include "task.h"

bool TryCompleteWaitPid(PageFrameContainer &frames,
		Thread *thread,
		uint64_t pid,
		uint64_t user_status_pointer,
		long &result);
void WakeChildWaiters(PageFrameContainer &frames);

#endif
