// Intrusive wait-queue and completion scaffolding for later SMP scheduler work.
#pragma once

#include <stddef.h>

#include "sync/atomic.hpp"
#include "sync/smp.hpp"

struct Thread;
struct cpu;

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

void wait_queue_enqueue_locked(WaitQueue& queue, Thread* thread);
[[nodiscard]] Thread* wait_queue_dequeue_locked(WaitQueue& queue);
[[nodiscard]] Thread* wait_queue_dequeue_all_locked(WaitQueue& queue);
[[nodiscard]] bool wait_queue_empty_locked(const WaitQueue& queue);
void wait_queue_enqueue(WaitQueue& queue, Thread* thread);
[[nodiscard]] Thread* wait_queue_dequeue(WaitQueue& queue);
[[nodiscard]] Thread* wait_queue_dequeue_all(WaitQueue& queue);
[[nodiscard]] bool wait_queue_empty(WaitQueue& queue);
[[nodiscard]] size_t wait_queue_count(WaitQueue& queue);
[[nodiscard]] size_t wait_queue_wake_one(WaitQueue& queue, cpu* target = nullptr);
[[nodiscard]] size_t wait_queue_wake_all(WaitQueue& queue, cpu* target = nullptr);
void completion_reset(Completion& completion);
[[nodiscard]] bool completion_done(const Completion& completion);
[[nodiscard]] bool completion_signal(Completion& completion);
