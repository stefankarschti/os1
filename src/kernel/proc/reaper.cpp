// Deferred thread/process reaping. The active kernel stack cannot be freed by
// the thread currently running on it, so reclamation happens when another thread
// re-enters the kernel.
#include "proc/thread.h"

#include "handoff/memory_layout.h"

void reapDeadThreads(PageFrameContainer &frames)
{
	Thread *active = currentThread();
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		Thread *thread = threadTable + i;
		if((thread == active) || (ThreadState::Dying != thread->state))
		{
			continue;
		}

		// Thread teardown is deferred until some *other* thread enters the kernel.
		// That keeps us from freeing the current kernel stack out from under the CPU
		// while it is still executing on it.
		for(size_t page = 0; page < kKernelThreadStackPages; ++page)
		{
			frames.Free(thread->kernel_stack_base + page * kPageSize);
		}

		Process *owner = thread->process;
		clearThread(thread);

		if(owner && !processHasThreads(owner))
		{
			if(ProcessState::Zombie == owner->state)
			{
				continue;
			}

			reapProcess(owner, frames);
		}
	}

	relinkRunnableThreads();
}