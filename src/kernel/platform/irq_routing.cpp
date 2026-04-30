// Interrupt routing policy for ISA IRQs after ACPI topology discovery.
#include "platform/irq_routing.hpp"

#include "arch/x86_64/apic/ioapic.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "debug/debug.hpp"
#include "platform/irq_registry.hpp"
#include "platform/platform.hpp"
#include "platform/state.hpp"
#include "platform/topology.hpp"

bool add_legacy_interrupt_override(uint8_t bus_irq, uint32_t global_irq, uint16_t flags)
{
    if(g_platform.override_count >= kPlatformMaxInterruptOverrides)
    {
        debug("platform: interrupt override table full")();
        return false;
    }
    InterruptOverride& entry = g_platform.overrides[g_platform.override_count++];
    entry.bus_irq = bus_irq;
    entry.flags = flags;
    entry.global_irq = global_irq;
    return true;
}

bool platform_route_isa_irq(DeviceId owner, int bus_irq, uint8_t vector)
{
    if((nullptr == ioapic) || !ismp)
    {
        return false;
    }
    if(!interrupt_vector_is_external(vector))
    {
        debug("platform: invalid ISA route vector 0x")(vector, 16, 2)();
        return false;
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

    if(!ioapic_enable_gsi(global_irq, vector, flags))
    {
        return false;
    }

    return platform_register_isa_irq_route(
        owner, static_cast<uint8_t>(bus_irq), global_irq, flags, vector);
}
