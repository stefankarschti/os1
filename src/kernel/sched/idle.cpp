// Idle-loop implementation used when no runnable user thread is available.
#include "sched/idle.h"

#include "console/console.h"

void KernelIdleThread()
{
	static bool announced = false;
	if(!announced)
	{
		announced = true;
		WriteConsoleLine("idle thread online");
	}
	for(;;)
	{
		// The kernel has no deferred work queue yet, so the idle thread sleeps until
		// an interrupt returns control to the scheduler.
		asm volatile("sti; hlt");
	}
}