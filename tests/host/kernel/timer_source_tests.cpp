#include "arch/x86_64/interrupt/interrupt.hpp"
#include "handoff/memory_layout.h"
#include "core/timer_source.hpp"
#include "mm/kmem.hpp"
#include "platform/irq_registry.hpp"
#include "platform/state.hpp"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>

namespace
{
constexpr uint64_t kArenaBytes = 4ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x100000;

PageFrameContainer make_frames()
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

void reset_state()
{
    platform_reset_state();
    timer_source_set_scheduler(SchedulerTimerSource::Pit);
}

struct ScopedTimerSourceCleanup
{
    ~ScopedTimerSourceCleanup()
    {
        platform_reset_state();
        timer_source_set_scheduler(SchedulerTimerSource::Pit);
    }
};
}  // namespace

TEST(TimerSource, PitModeMatchesLegacyTimerOnly)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = make_frames();
    kmem_init(frames);
    ScopedTimerSourceCleanup cleanup;
    reset_state();

    EXPECT_TRUE(timer_vector_is_scheduler_tick(static_cast<uint8_t>(T_IRQ0 + IRQ_TIMER)));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(T_LTIMER));
}

TEST(TimerSource, LapicModeMatchesLocalApicTimerRoute)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = make_frames();
    kmem_init(frames);
    ScopedTimerSourceCleanup cleanup;
    reset_state();
    timer_source_set_scheduler(SchedulerTimerSource::Lapic);

    uint8_t vector = 0;
    ASSERT_TRUE(
        platform_allocate_local_apic_irq_route(DeviceId{DeviceBus::Platform, 2}, T_LTIMER, vector));
    EXPECT_TRUE(timer_vector_is_scheduler_tick(vector));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(T_LTIMER));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(static_cast<uint8_t>(T_IRQ0 + IRQ_TIMER)));
}

TEST(TimerSource, LapicModeRejectsNonTimerLocalApicRoute)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = make_frames();
    kmem_init(frames);
    ScopedTimerSourceCleanup cleanup;
    reset_state();
    timer_source_set_scheduler(SchedulerTimerSource::Lapic);

    ASSERT_TRUE(platform_register_local_apic_irq_route(DeviceId{DeviceBus::Platform, 3}, T_LERROR, T_LERROR));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(T_LERROR));
}