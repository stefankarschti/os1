// Intrusive wait-queue and completion scaffolding for later SMP scheduler work.
#pragma once

#include <stddef.h>

#include "proc/thread.hpp"
#include "sync/atomic.hpp"
#include "sync/smp.hpp"

struct WaitQueue
{
    constexpr explicit WaitQueue(const char* queue_name = "wait-queue")
        : lock(queue_name), head(nullptr), name(queue_name)
    {
    }

    Spinlock lock;
    Thread* head;
    const char* name;
};

struct Completion
{
    constexpr explicit Completion(const char* completion_name = "completion")
        : waiters(completion_name), done(false)
    {
    }

    WaitQueue waiters;
    volatile bool done;
};

inline void wait_queue_enqueue_locked(WaitQueue& queue, Thread* thread)
{
    if(nullptr == thread)
    {
        return;
    }

    if(nullptr == queue.head)
    {
        thread->wait_link = nullptr;
        queue.head = thread;
        return;
    }

    Thread* tail = queue.head;
    for(Thread* current = queue.head; nullptr != current; current = current->wait_link)
    {
        if(current == thread)
        {
            return;
        }
        tail = current;
    }

    thread->wait_link = nullptr;
    tail->wait_link = thread;
}

[[nodiscard]] inline Thread* wait_queue_dequeue_locked(WaitQueue& queue)
{
    Thread* thread = queue.head;
    if(nullptr != thread)
    {
        queue.head = thread->wait_link;
        thread->wait_link = nullptr;
    }
    return thread;
}

[[nodiscard]] inline Thread* wait_queue_dequeue_all_locked(WaitQueue& queue)
{
    Thread* first = queue.head;
    queue.head = nullptr;
    return first;
}

[[nodiscard]] inline bool wait_queue_empty_locked(const WaitQueue& queue)
{
    return nullptr == queue.head;
}

inline void wait_queue_enqueue(WaitQueue& queue, Thread* thread)
{
    IrqSpinGuard guard(queue.lock);
    wait_queue_enqueue_locked(queue, thread);
}

[[nodiscard]] inline Thread* wait_queue_dequeue(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_dequeue_locked(queue);
}

[[nodiscard]] inline Thread* wait_queue_dequeue_all(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_dequeue_all_locked(queue);
}

[[nodiscard]] inline bool wait_queue_empty(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_empty_locked(queue);
}

[[nodiscard]] inline size_t wait_queue_count(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    size_t count = 0;
    for(Thread* thread = queue.head; nullptr != thread; thread = thread->wait_link)
    {
        ++count;
    }
    return count;
}

inline void completion_reset(Completion& completion)
{
    atomic_store_release(&completion.done, false);
}

[[nodiscard]] inline bool completion_done(const Completion& completion)
{
    return atomic_load_acquire(&completion.done);
}

[[nodiscard]] inline bool completion_signal(Completion& completion)
{
    return !atomic_exchange(&completion.done, true);
}
