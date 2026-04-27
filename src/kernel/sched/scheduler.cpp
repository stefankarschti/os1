// Round-robin scheduler handoff wrapper around the current task-table API.
#include "sched/scheduler.hpp"

#include "core/kernel_state.hpp"
#include "proc/thread.hpp"
#include "syscall/wait.hpp"

Thread* schedule_next(bool keep_current)
{
    reap_dead_threads(page_frames);
    wake_child_waiters(page_frames);
    Thread* current = current_thread();
    if(keep_current && current)
    {
        mark_thread_ready(current);
    }

    Thread* next = next_runnable_thread(current);
    if(nullptr == next)
    {
        next = idle_thread();
    }
    return next;
}