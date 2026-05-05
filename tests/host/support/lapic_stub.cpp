// Host-test stubs for LAPIC entry points referenced by timer_source.cpp.
// The host harness does not exercise the local APIC; these stubs let
// timer_source link cleanly when included in the support library.
#include "arch/x86_64/apic/lapic.hpp"

volatile uint32_t* lapic = nullptr;

void lapic_init(void) {}

bool lapic_timer_available(void)
{
    return false;
}

void lapic_timer_prepare_calibration(uint32_t)
{
}

uint32_t lapic_timer_current_count(void)
{
    return 0;
}

bool lapic_timer_start_periodic(uint8_t, uint32_t)
{
    return false;
}

void lapic_timer_mask(void) {}

void lapic_eoi(void) {}

void lapic_err_intr(void) {}

void lapic_start_cpu(uint8_t, uint32_t) {}
