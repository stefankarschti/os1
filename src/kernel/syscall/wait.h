// waitpid syscall helpers. The scheduler and process reaper call back here to
// wake parent threads when child process state changes.
#ifndef OS1_KERNEL_SYSCALL_WAIT_H
#define OS1_KERNEL_SYSCALL_WAIT_H

#include <stdint.h>

#include "mm/page_frame.h"
#include "proc/thread.h"

// Try to complete waitpid for `thread`, writing status to user memory when done.
bool TryCompleteWaitPid(PageFrameContainer &frames,
		Thread *thread,
		uint64_t pid,
		uint64_t user_status_pointer,
		long &result);
// Wake threads blocked on child-exit waits after a process exits or reaps.
void WakeChildWaiters(PageFrameContainer &frames);

#endif // OS1_KERNEL_SYSCALL_WAIT_H
