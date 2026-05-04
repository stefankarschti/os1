#include "arch/x86_64/interrupt/interrupt.hpp"
#include "core/timer_source.hpp"
#include "platform/irq_registry.hpp"
#include "platform/state.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace
{
void reset_state()
{
    std::memset(&g_platform, 0, sizeof(g_platform));
    timer_source_set_scheduler(SchedulerTimerSource::Pit);
}
}  // namespace

TEST(TimerSource, PitModeMatchesLegacyTimerOnly)
{
    reset_state();

    EXPECT_TRUE(timer_vector_is_scheduler_tick(static_cast<uint8_t>(T_IRQ0 + IRQ_TIMER)));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(T_LTIMER));
}

TEST(TimerSource, LapicModeMatchesLocalApicTimerRoute)
{
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
    reset_state();
    timer_source_set_scheduler(SchedulerTimerSource::Lapic);

    ASSERT_TRUE(platform_register_local_apic_irq_route(DeviceId{DeviceBus::Platform, 3}, T_LERROR, T_LERROR));
    EXPECT_FALSE(timer_vector_is_scheduler_tick(T_LERROR));
}