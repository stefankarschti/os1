// Compatibility fallback for BIOS boots that do not provide usable ACPI tables.
#include "platform/legacy_mp.h"

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/apic/lapic.h"
#include "arch/x86_64/apic/mp.h"
#include "arch/x86_64/interrupt/interrupt.h"
#include "debug/debug.h"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.h"
#include "mm/virtual_memory.h"
#include "platform/irq_routing.h"
#include "platform/state.h"
#include "platform/topology.h"

bool UseLegacyMpFallback(VirtualMemory &kernel_vm)
{
	ResetMpStateForFallback();
	mp_init();
	if(!ismp || (nullptr == lapic) || (nullptr == ioapic))
	{
		debug("platform: legacy MP fallback unavailable")();
		return false;
	}
	if(!MapIdentityRange(kernel_vm, reinterpret_cast<uint64_t>(lapic), kPageSize)
		|| !MapIdentityRange(kernel_vm, reinterpret_cast<uint64_t>(ioapic), kPageSize))
	{
		return false;
	}
	g_platform.used_legacy_mp_fallback = true;
	g_platform.acpi_active = false;
	g_platform.override_count = 0;
	if(!AddLegacyInterruptOverride(IRQ_TIMER, 2, 0))
	{
		return false;
	}
	ioapic_set_primary(0);
	g_platform.initialized = true;
	debug("platform: legacy MP fallback active")();
	return true;
}