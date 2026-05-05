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
}  // namespace

Thread* next_runnable_thread(Thread* after)
{
    relink_runnable_threads();

    Thread* start = first_thread();
    if(nullptr != after)
    {
        for(Thread* candidate = first_thread(); nullptr != candidate; candidate = next_thread(candidate))
        {
            if(candidate != after)
            {
                continue;
            }

            start = (nullptr != candidate->registry_next) ? candidate->registry_next : first_thread();
            break;
        }
    }

    Thread* idle_candidate = nullptr;
    for(Thread* candidate = start; nullptr != candidate; candidate = next_thread(candidate))
    {
        if(!schedulable_on_current_cpu(candidate))
        {
            continue;
        }
        if(candidate == idle_thread())
        {
            if(nullptr == idle_candidate)
            {
                idle_candidate = candidate;
            }
            continue;
        }
        return candidate;
    }

    if(start != first_thread())
    {
        for(Thread* candidate = first_thread(); candidate != start; candidate = next_thread(candidate))
        {
            if(!schedulable_on_current_cpu(candidate))
            {
                continue;
            }
            if(candidate == idle_thread())
            {
                if(nullptr == idle_candidate)
                {
                    idle_candidate = candidate;
                }
                continue;
            }
            return candidate;
        }
    }

    return idle_candidate;
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
