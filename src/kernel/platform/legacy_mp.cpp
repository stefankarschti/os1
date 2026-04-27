// Compatibility fallback for BIOS boots that do not provide usable ACPI tables.
#include "platform/legacy_mp.hpp"

#include "arch/x86_64/apic/ioapic.hpp"
#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/apic/mp.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "mm/virtual_memory.hpp"
#include "platform/irq_routing.hpp"
#include "platform/state.hpp"
#include "platform/topology.hpp"

bool use_legacy_mp_fallback(VirtualMemory& kernel_vm)
{
    reset_mp_state_for_fallback();
    mp_init();
    if(!ismp || (nullptr == lapic) || (nullptr == ioapic))
    {
        debug("platform: legacy MP fallback unavailable")();
        return false;
    }
    if(!map_identity_range(kernel_vm, reinterpret_cast<uint64_t>(lapic), kPageSize) ||
       !map_identity_range(kernel_vm, reinterpret_cast<uint64_t>(ioapic), kPageSize))
    {
        return false;
    }
    g_platform.used_legacy_mp_fallback = true;
    g_platform.acpi_active = false;
    g_platform.override_count = 0;
    if(!add_legacy_interrupt_override(IRQ_TIMER, 2, 0))
    {
        return false;
    }
    ioapic_set_primary(0);
    g_platform.initialized = true;
    debug("platform: legacy MP fallback active")();
    return true;
}