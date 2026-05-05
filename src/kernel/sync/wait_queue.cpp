#include "sync/wait_queue.hpp"

#include "proc/thread.hpp"

void wait_queue_enqueue_locked(WaitQueue& queue, Thread* thread)
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

Thread* wait_queue_dequeue_locked(WaitQueue& queue)
{
    Thread* thread = queue.head;
    if(nullptr != thread)
    {
        queue.head = thread->wait_link;
        thread->wait_link = nullptr;
    }
    return thread;
}

Thread* wait_queue_dequeue_all_locked(WaitQueue& queue)
{
    Thread* first = queue.head;
    queue.head = nullptr;
    return first;
}

bool wait_queue_empty_locked(const WaitQueue& queue)
{
    return nullptr == queue.head;
}

void wait_queue_enqueue(WaitQueue& queue, Thread* thread)
{
    IrqSpinGuard guard(queue.lock);
    wait_queue_enqueue_locked(queue, thread);
}

Thread* wait_queue_dequeue(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_dequeue_locked(queue);
}

Thread* wait_queue_dequeue_all(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_dequeue_all_locked(queue);
}

bool wait_queue_empty(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    return wait_queue_empty_locked(queue);
}

size_t wait_queue_count(WaitQueue& queue)
{
    IrqSpinGuard guard(queue.lock);
    size_t count = 0;
    for(Thread* thread = queue.head; nullptr != thread; thread = thread->wait_link)
    {
        ++count;
    }
    return count;
}

size_t wait_queue_wake_one(WaitQueue& queue, cpu* target)
{
    Thread* thread = wait_queue_dequeue(queue);
    if(nullptr == thread)
    {
        return 0u;
    }

    wake_blocked_thread(thread, target);
    return 1u;
}

size_t wait_queue_wake_all(WaitQueue& queue, cpu* target)
{
    Thread* thread = wait_queue_dequeue_all(queue);
    size_t count = 0;
    while(nullptr != thread)
    {
        Thread* next = thread->wait_link;
        thread->wait_link = nullptr;
        wake_blocked_thread(thread, target);
        thread = next;
        ++count;
    }
    return count;
}

void completion_reset(Completion& completion)
{
    atomic_store_release(&completion.done, false);
}

bool completion_done(const Completion& completion)
{
    return atomic_load_acquire(&completion.done);
}

bool completion_signal(Completion& completion)
{
    const bool first_signal = !atomic_exchange(&completion.done, true);
    (void)wait_queue_wake_all(completion.waiters);
    return first_signal;
}