// Deferred thread/process reaping. The active kernel stack cannot be freed by
// the thread currently running on it, so reclamation happens when another thread
// re-enters the kernel.
#include "handoff/memory_layout.h"
#include "proc/thread.hpp"
#include "syscall/wait.hpp"

void reap_dead_threads(PageFrameContainer& frames)
{
    Thread* active = current_thread();
    for(;;)
    {
        Thread* thread = nullptr;
        {
            IrqSpinGuard guard(g_thread_registry_lock);
            for(Thread* candidate = first_thread(); nullptr != candidate; candidate = next_thread(candidate))
            {
                if((candidate != active) && (ThreadState::Dying == candidate->state))
                {
                    thread = candidate;
                    break;
                }
            }
        }

        if(nullptr == thread)
        {
            break;
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
                wake_child_waiters(frames, owner->parent);
                continue;
            }

            reap_process(owner, frames);
        }
    }

    relink_runnable_threads();
}
