// Interrupt routing policy for ISA IRQs after ACPI or legacy MP discovery.
#include "platform/irq_routing.h"

#include "arch/x86_64/apic/ioapic.h"
#include "arch/x86_64/apic/mp.h"
#include "debug/debug.h"
#include "platform/platform.h"
#include "platform/state.h"

bool AddLegacyInterruptOverride(uint8_t bus_irq, uint32_t global_irq, uint16_t flags)
{
	if(g_platform.override_count >= kPlatformMaxInterruptOverrides)
	{
		debug("platform: interrupt override table full")();
		return false;
	}
	InterruptOverride &entry = g_platform.overrides[g_platform.override_count++];
	entry.bus_irq = bus_irq;
	entry.flags = flags;
	entry.global_irq = global_irq;
	return true;
}

bool platform_enable_isa_irq(int bus_irq, int irq)
{
	if((nullptr == ioapic) || !ismp)
	{
		return false;
	}
	if(irq < 0)
	{
		irq = bus_irq;
	}

	uint32_t global_irq = static_cast<uint32_t>(bus_irq);
	uint16_t flags = 0;
	for(size_t i = 0; i < g_platform.override_count; ++i)
	{
		if(g_platform.overrides[i].bus_irq == static_cast<uint8_t>(bus_irq))
		{
			global_irq = g_platform.overrides[i].global_irq;
			flags = g_platform.overrides[i].flags;
			break;
		}
	}

	return ioapic_enable_gsi(global_irq, irq, flags);
}