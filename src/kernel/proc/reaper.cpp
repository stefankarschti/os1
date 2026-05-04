// Deferred thread/process reaping. The active kernel stack cannot be freed by
// the thread currently running on it, so reclamation happens when another thread
// re-enters the kernel.
#include "handoff/memory_layout.h"
#include "proc/thread.hpp"

void reap_dead_threads(PageFrameContainer& frames)
{
    if(nullptr == threadTable)
    {
        return;
    }

    Thread* active = current_thread();
    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        Thread* thread = threadTable + i;
        if((thread == active) || (ThreadState::Dying != thread->state))
        {
            continue;
        }

        // Thread teardown is deferred until some *other* thread enters the kernel.
        // That keeps us from freeing the current kernel stack out from under the CPU
        // while it is still executing on it.
        for(size_t page = 0; page < kKernelThreadStackPages; ++page)
        {
            frames.free(thread->kernel_stack_base + page * kPageSize);
        }

        Process* owner = thread->process;
        clear_thread(thread);

        if(owner && !process_has_threads(owner))
        {
            if(ProcessState::Zombie == owner->state)
            {
                continue;
            }

            reap_process(owner, frames);
        }
    }

    relink_runnable_threads();
}
