// Simple runnable selection in thread-registry order. This file owns scheduler
// policy; object lifetime remains in proc/thread.cpp and proc/reaper.cpp.
#include "arch/x86_64/cpu/cpu.hpp"
#include "proc/thread.hpp"

namespace
{
[[nodiscard]] bool has_runnable_state(const Thread* thread)
{
    return (nullptr != thread) &&
           ((ThreadState::Ready == thread->state) || (ThreadState::Running == thread->state));
}

[[nodiscard]] bool is_idle_thread_for_another_cpu(const Thread* thread)
{
    if(nullptr == thread)
    {
        return false;
    }

    const cpu* current = cpu_cur();
    for(cpu* owner = g_cpu_boot; nullptr != owner; owner = owner->next)
    {
        if(owner->idle_thread == thread)
        {
            return owner != current;
        }
    }
    return false;
}

[[nodiscard]] bool schedulable_on_current_cpu(const Thread* thread)
{
    return has_runnable_state(thread) && !is_idle_thread_for_another_cpu(thread);
}

Thread* dequeue_ready_thread(cpu* owner)
{
    if(nullptr == owner)
    {
        return nullptr;
    }

    IrqSpinGuard guard(owner->runq.lock);
    Thread* thread = owner->runq.head;
    if(nullptr == thread)
    {
        return nullptr;
    }

    owner->runq.head = thread->next;
    if(nullptr == owner->runq.head)
    {
        owner->runq.tail = nullptr;
    }
    thread->next = nullptr;
    thread->run_queue_cpu = nullptr;
    if(0u != owner->runq.length)
    {
        --owner->runq.length;
    }
    ++owner->dequeue_count;
    return thread;
}
}  // namespace

Thread* next_runnable_thread(Thread* after)
{
    (void)after;
    return dequeue_ready_thread(cpu_cur());
}

size_t runnable_thread_count(void)
{
    size_t count = 0;
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        if(schedulable_on_current_cpu(thread))
        {
            ++count;
        }
    }
    return count;
}

size_t cpu_run_queue_length(const cpu* owner)
{
    return (nullptr != owner) ? owner->runq.length : 0u;
}

Thread* first_runnable_user_thread(void)
{
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        if(thread->user_mode &&
           ((ThreadState::Ready == thread->state) || (ThreadState::Running == thread->state)))
        {
            return thread;
        }
    }
    return nullptr;
}
