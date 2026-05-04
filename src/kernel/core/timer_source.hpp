// Scheduler timer source selection shared by IRQ dispatch and boot bring-up.
#pragma once

#include <stdint.h>

enum class SchedulerTimerSource : uint8_t
{
    Pit = 0,
    Lapic = 1,
};

void timer_source_set_scheduler(SchedulerTimerSource source);
SchedulerTimerSource timer_source_scheduler();
bool timer_vector_is_scheduler_tick(uint8_t vector);