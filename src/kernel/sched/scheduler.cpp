// Round-robin scheduler handoff wrapper around the current task-table API.
#include "sched/scheduler.hpp"

#include "core/kernel_state.hpp"
#include "debug/event_ring.hpp"
#include "proc/thread.hpp"
#include "syscall/wait.hpp"

namespace
{
[[nodiscard]] uint64_t thread_pid(const Thread* thread)
{
    return (thread && thread->process) ? thread->process->pid : 0;
}

[[nodiscard]] uint64_t thread_tid(const Thread* thread)
{
    return thread ? thread->tid : 0;
}
}  // namespace

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
    if(next != current)
    {
        kernel_event::record(OS1_KERNEL_EVENT_SCHED_TRANSITION,
                             0,
                             thread_pid(current),
                             thread_tid(current),
                             thread_pid(next),
                             thread_tid(next));
    }
    return next;
}
