#include "sync/wait_queue.hpp"

#include <gtest/gtest.h>

namespace
{
Thread make_thread(uint64_t tid)
{
    Thread thread{};
    thread.tid = tid;
    thread.state = ThreadState::Blocked;
    return thread;
}
}  // namespace

TEST(WaitQueue, EnqueuesAndDequeuesInFifoOrder)
{
    WaitQueue queue("test-wait-queue");
    Thread first = make_thread(1);
    Thread second = make_thread(2);
    Thread third = make_thread(3);

    EXPECT_TRUE(wait_queue_empty(queue));

    wait_queue_enqueue(queue, &first);
    wait_queue_enqueue(queue, &second);
    wait_queue_enqueue(queue, &third);

    EXPECT_EQ(3u, wait_queue_count(queue));
    EXPECT_EQ(&first, wait_queue_dequeue(queue));
    EXPECT_EQ(&second, wait_queue_dequeue(queue));
    EXPECT_EQ(&third, wait_queue_dequeue(queue));
    EXPECT_EQ(nullptr, wait_queue_dequeue(queue));
    EXPECT_TRUE(wait_queue_empty(queue));
    EXPECT_EQ(nullptr, first.wait_link);
    EXPECT_EQ(nullptr, second.wait_link);
    EXPECT_EQ(nullptr, third.wait_link);
}

TEST(WaitQueue, DequeueAllDetachesListInOrder)
{
    WaitQueue queue("test-wait-queue");
    Thread first = make_thread(1);
    Thread second = make_thread(2);
    Thread third = make_thread(3);

    wait_queue_enqueue(queue, &first);
    wait_queue_enqueue(queue, &second);
    wait_queue_enqueue(queue, &third);

    Thread* list = wait_queue_dequeue_all(queue);
    EXPECT_TRUE(wait_queue_empty(queue));
    ASSERT_EQ(&first, list);
    ASSERT_EQ(&second, list->wait_link);
    ASSERT_EQ(&third, list->wait_link->wait_link);
    EXPECT_EQ(nullptr, list->wait_link->wait_link->wait_link);
}

TEST(WaitQueue, DuplicateEnqueueIsIgnored)
{
    WaitQueue queue("test-wait-queue");
    Thread thread = make_thread(1);

    wait_queue_enqueue(queue, &thread);
    wait_queue_enqueue(queue, &thread);

    EXPECT_EQ(1u, wait_queue_count(queue));
    EXPECT_EQ(&thread, wait_queue_dequeue(queue));
    EXPECT_EQ(nullptr, wait_queue_dequeue(queue));
}

TEST(WaitQueue, LockedHelpersSupportExplicitGuard)
{
    WaitQueue queue("test-wait-queue");
    Thread first = make_thread(1);
    Thread second = make_thread(2);

    {
        IrqSpinGuard guard(queue.lock);
        wait_queue_enqueue_locked(queue, &first);
        wait_queue_enqueue_locked(queue, &second);
        EXPECT_FALSE(wait_queue_empty_locked(queue));
        EXPECT_EQ(&first, wait_queue_dequeue_locked(queue));
    }

    EXPECT_EQ(&second, wait_queue_dequeue(queue));
    EXPECT_TRUE(wait_queue_empty(queue));
}

TEST(Completion, SignalIsOneShotAndResettable)
{
    Completion completion("test-completion");

    EXPECT_FALSE(completion_done(completion));
    EXPECT_TRUE(completion_signal(completion));
    EXPECT_TRUE(completion_done(completion));
    EXPECT_FALSE(completion_signal(completion));

    completion_reset(completion);

    EXPECT_FALSE(completion_done(completion));
    EXPECT_TRUE(completion_signal(completion));
    EXPECT_TRUE(completion_done(completion));
}
