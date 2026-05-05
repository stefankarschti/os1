// Round-robin scheduler handoff wrapper around the current task-table API.
#include "sched/scheduler.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
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
    if(cpu_on_boot())
    {
        reap_dead_threads(page_frames);
    }
    Thread* current = current_thread();
    cpu* local_cpu = cpu_cur();
    if(keep_current && current && (current != idle_thread()) &&
       (ThreadState::Running == current->state))
    {
        mark_thread_ready(current, local_cpu);
    }

    local_cpu->reschedule_pending = 0;
    Thread* next = next_runnable_thread(current);
    if(nullptr == next)
    {
        next = local_cpu->idle_thread;
    }
    if(nullptr != next)
    {
        next->state = ThreadState::Running;
        if((nullptr != next->process) && (ProcessState::Dying != next->process->state))
        {
            next->process->state = ProcessState::Running;
        }
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
