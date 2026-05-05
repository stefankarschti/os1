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

// Phase 3: shared LAPIC timer calibration produced once on the BSP and reused
// per-CPU. `valid` reflects whether the BSP completed HPET calibration and
// successfully programmed its own LAPIC timer.
struct LapicTimerCalibration
{
    uint8_t vector;
    uint32_t initial_count;
    uint32_t frequency_hz;
    bool valid;
};

void timer_source_set_lapic_calibration(uint8_t vector,
                                        uint32_t initial_count,
                                        uint32_t frequency_hz);
LapicTimerCalibration timer_source_lapic_calibration();

// Rollback gate: when false, APs do not start their LAPIC timer.
void timer_source_set_ap_timer_enabled(bool enabled);
bool timer_source_ap_timer_enabled();

// Start the LAPIC timer on the calling CPU using the cached calibration. The
// caller must already have an IDT loaded. Returns true on successful arming.
bool cpu_start_local_apic_timer();