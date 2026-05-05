#include "proc/process.hpp"

#include "handoff/memory_layout.h"
#include "mm/kmem.hpp"
#include "proc/thread.hpp"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace
{
constexpr uint64_t kArenaBytes = 32ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x300000;
constexpr size_t kRegistryGrowthCount = 48;

PageFrameContainer initialized_frames()
{
    std::array<BootMemoryRegion, 1> regions{{
        {
            .physical_start = 0,
            .length = kArenaBytes,
            .type = BootMemoryType::Usable,
            .attributes = 0,
        },
    }};

    PageFrameContainer frames;
    EXPECT_TRUE(frames.initialize(regions, kBitmapPhysical, kPageFrameBitmapQwordLimit));
    return frames;
}

void noop_thread_entry()
{
}
}  // namespace

TEST(TaskRegistry, ProcessesGrowPastLegacyLimit)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ASSERT_TRUE(init_tasks(frames));

    std::vector<Process*> processes;
    processes.reserve(kRegistryGrowthCount);
    for(size_t index = 0; index < kRegistryGrowthCount; ++index)
    {
        Process* process = create_user_process("worker", 0);
        ASSERT_NE(nullptr, process);
        processes.push_back(process);
    }

    size_t count = 0;
    for(Process* process = first_process(); nullptr != process; process = next_process(process))
    {
        ++count;
    }

    EXPECT_EQ(processes.size(), count);
    EXPECT_EQ(processes.front(), first_process());

    for(Process* process = first_process(); nullptr != process;)
    {
        Process* next = next_process(process);
        EXPECT_TRUE(reap_process(process, frames));
        process = next;
    }

    EXPECT_EQ(nullptr, first_process());
}

TEST(TaskRegistry, ThreadsGrowPastLegacyLimitAndReapCleanly)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ASSERT_TRUE(init_tasks(frames));
    Process* kernel_process = create_kernel_process(0);
    ASSERT_NE(nullptr, kernel_process);

    std::vector<Thread*> threads;
    threads.reserve(kRegistryGrowthCount);
    for(size_t index = 0; index < kRegistryGrowthCount; ++index)
    {
        Thread* thread = create_kernel_thread(kernel_process, noop_thread_entry, frames);
        ASSERT_NE(nullptr, thread);
        threads.push_back(thread);
    }

    size_t count = 0;
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        ++count;
    }

    ASSERT_EQ(threads.size(), count);
    ASSERT_EQ(threads.front(), idle_thread());
    EXPECT_EQ(threads.size(), runnable_thread_count());
    ASSERT_LT(2u, threads.size());
    EXPECT_EQ(threads[1], next_runnable_thread(nullptr));
    EXPECT_EQ(threads[2], next_runnable_thread(nullptr));

    for(Thread* thread : threads)
    {
        thread->state = ThreadState::Dying;
    }

    reap_dead_threads(frames);

    EXPECT_EQ(nullptr, first_thread());
    EXPECT_EQ(nullptr, first_process());
    EXPECT_EQ(0u, runnable_thread_count());
}

TEST(TaskRegistry, ZombieProcessSurvivesThreadReapUntilWaitpidCanCollectIt)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ASSERT_TRUE(init_tasks(frames));

    Process* parent = create_user_process("parent", 0);
    ASSERT_NE(nullptr, parent);
    Process* child = create_user_process("child", 0);
    ASSERT_NE(nullptr, child);
    child->parent = parent;

    Thread* child_thread = create_kernel_thread(child, noop_thread_entry, frames);
    ASSERT_NE(nullptr, child_thread);

    set_current_thread(child_thread);
    mark_current_thread_dying(7);
    set_current_thread(nullptr);

    reap_dead_threads(frames);

    EXPECT_EQ(nullptr, first_thread());
    ASSERT_NE(nullptr, first_process());
    EXPECT_EQ(ProcessState::Zombie, child->state);
    size_t process_count = 0;
    for(Process* process = first_process(); nullptr != process; process = next_process(process))
    {
        ++process_count;
    }
    EXPECT_EQ(2u, process_count);

    child->parent = nullptr;
    child->state = ProcessState::Dying;
    EXPECT_TRUE(reap_process(child, frames));
    EXPECT_TRUE(reap_process(parent, frames));
    EXPECT_EQ(nullptr, first_process());
}