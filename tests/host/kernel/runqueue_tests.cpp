#include <gtest/gtest.h>

#ifdef assert
#undef assert
#endif

#ifdef static_assert
#undef static_assert
#endif

#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/apic/ipi.hpp"
#include "handoff/memory_layout.h"
#include "mm/kmem.hpp"
#include "proc/thread.hpp"
#include "support/lapic_stub.hpp"
#include "support/physical_memory.hpp"

#include <array>

#ifdef static_assert
#undef static_assert
#endif

namespace
{
constexpr uint64_t kArenaBytes = 16ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x300000;

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

void idle_entry()
{
}

struct HostCpuScope
{
    explicit HostCpuScope(cpu* ap) : saved_next(g_cpu_boot->next), saved_current(g_cpu_host_current)
    {
        g_cpu_boot->next = ap;
        g_cpu_host_current = g_cpu_boot;
    }

    ~HostCpuScope()
    {
        g_cpu_boot->next = saved_next;
        g_cpu_boot->idle_thread = nullptr;
        g_cpu_host_current = saved_current;
    }

    cpu* saved_next;
    cpu* saved_current;
};
}  // namespace

TEST(RunQueue, PopsReadyThreadsInFifoOrder)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ASSERT_TRUE(init_tasks(frames));
    Process* kernel_process = create_kernel_process(0);
    ASSERT_NE(nullptr, kernel_process);

    Thread* idle = create_kernel_thread(kernel_process, idle_entry, frames);
    Thread* worker_a = create_kernel_thread(kernel_process, idle_entry, frames);
    Thread* worker_b = create_kernel_thread(kernel_process, idle_entry, frames);

    ASSERT_NE(nullptr, idle);
    ASSERT_NE(nullptr, worker_a);
    ASSERT_NE(nullptr, worker_b);
    EXPECT_EQ(idle, idle_thread());
    EXPECT_EQ(2u, cpu_run_queue_length(g_cpu_boot));

    EXPECT_EQ(worker_a, next_runnable_thread(nullptr));
    EXPECT_EQ(worker_b, next_runnable_thread(nullptr));
    EXPECT_EQ(nullptr, next_runnable_thread(nullptr));
    EXPECT_EQ(0u, cpu_run_queue_length(g_cpu_boot));
}

TEST(RunQueue, DefaultWakeTargetUsesAssignedCpu)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    cpu ap_cpu{};
    ap_cpu.self = &ap_cpu;
    ap_cpu.id = 1;
    ap_cpu.booted = 1;
    ap_cpu.magic = CPU_MAGIC;

    HostCpuScope cpu_scope(&ap_cpu);
    ASSERT_TRUE(init_tasks(frames));
    ASSERT_TRUE(ipi_initialize());
    lapic_stub_reset();
    Process* kernel_process = create_kernel_process(0);
    ASSERT_NE(nullptr, kernel_process);
    ASSERT_NE(nullptr, create_idle_thread_for_cpu(kernel_process, g_cpu_boot, idle_entry, frames));
    ASSERT_NE(nullptr, create_idle_thread_for_cpu(kernel_process, &ap_cpu, idle_entry, frames));

    g_cpu_host_current = &ap_cpu;
    Process* user_process = create_user_process("worker", 0);
    ASSERT_NE(nullptr, user_process);
    Thread* blocked = create_user_thread(user_process, 0x1000, 0x2000, frames, false);
    ASSERT_NE(nullptr, blocked);
    EXPECT_EQ(&ap_cpu, blocked->scheduler_cpu);
    EXPECT_EQ(nullptr, blocked->run_queue_cpu);

    g_cpu_host_current = g_cpu_boot;
    mark_thread_ready(blocked);

    EXPECT_EQ(ThreadState::Ready, blocked->state);
    EXPECT_EQ(&ap_cpu, blocked->scheduler_cpu);
    EXPECT_EQ(&ap_cpu, blocked->run_queue_cpu);
    EXPECT_EQ(1u, cpu_run_queue_length(&ap_cpu));
    EXPECT_EQ(0u, cpu_run_queue_length(g_cpu_boot));
    EXPECT_EQ(1u, lapic_stub_icr_send_count());
    EXPECT_EQ(static_cast<uint32_t>(ap_cpu.id) << 24, lapic_stub_last_icr_high());
    EXPECT_EQ(ipi_reschedule_vector(), static_cast<uint8_t>(lapic_stub_last_icr_low()));

    g_cpu_host_current = &ap_cpu;
    EXPECT_EQ(blocked, next_runnable_thread(nullptr));
    EXPECT_EQ(0u, cpu_run_queue_length(&ap_cpu));
}