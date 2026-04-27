// Round-robin scheduler handoff wrapper around the current task-table API.
#include "sched/scheduler.h"

#include "core/kernel_state.h"
#include "syscall/wait.h"
#include "proc/thread.h"

Thread *ScheduleNext(bool keep_current)
{
	reapDeadThreads(page_frames);
	WakeChildWaiters(page_frames);
	Thread *current = currentThread();
	if(keep_current && current)
	{
		markThreadReady(current);
	}

	Thread *next = nextRunnableThread(current);
	if(nullptr == next)
	{
		next = idleThread();
	}
	return next;
}