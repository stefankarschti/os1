// IRQ route ownership registry. This keeps vector-to-device relationships
// explicit before the wider driver core exists.
#include "platform/irq_registry.hpp"

#include "arch/x86_64/interrupt/interrupt.hpp"
#include "arch/x86_64/interrupt/vector_allocator.hpp"
#include "debug/debug.hpp"
#include "platform/state.hpp"

namespace
{
[[nodiscard]] bool device_id_equal(DeviceId left, DeviceId right)
{
    return (left.bus == right.bus) && (left.index == right.index);
}

[[nodiscard]] bool route_kind_uses_dynamic_vector(IrqRouteKind kind)
{
    switch(kind)
    {
        case IrqRouteKind::LocalApic:
        case IrqRouteKind::Msi:
        case IrqRouteKind::Msix:
            return true;
        case IrqRouteKind::LegacyIsa:
            return false;
    }
    return false;
}

IrqRoute* reserve_route_slot(uint8_t vector)
{
    for(size_t i = 0; i < g_platform.irq_route_count; ++i)
    {
        IrqRoute& route = g_platform.irq_routes[i];
        if(route.active && (route.vector == vector))
        {
            return nullptr;
        }
    }
    if(g_platform.irq_route_count >= kPlatformMaxIrqRoutes)
    {
        debug("platform: IRQ route table full")();
        return nullptr;
    }
    IrqRoute& route = g_platform.irq_routes[g_platform.irq_route_count++];
    route = {};
    route.active = true;
    route.vector = vector;
    return &route;
}

bool register_route(IrqRouteKind kind,
                    DeviceId owner,
                    uint16_t source_id,
                    uint8_t source_irq,
                    uint32_t gsi,
                    uint16_t flags,
                    uint8_t vector)
{
    if(!interrupt_vector_is_external(vector))
    {
        debug("platform: rejected invalid IRQ vector 0x")(vector, 16, 2)();
        return false;
    }

    IrqRoute* route = reserve_route_slot(vector);
    if(nullptr == route)
    {
        return false;
    }

    route->kind = kind;
    route->owner = owner;
    route->source_id = source_id;
    route->source_irq = source_irq;
    route->gsi = gsi;
    route->flags = flags;
    return true;
}
}  // namespace

bool platform_register_isa_irq_route(DeviceId owner,
                                     uint8_t source_irq,
                                     uint32_t gsi,
                                     uint16_t flags,
                                     uint8_t vector)
{
    return register_route(IrqRouteKind::LegacyIsa, owner, 0, source_irq, gsi, flags, vector);
}

bool platform_register_local_apic_irq_route(DeviceId owner, uint16_t source_id, uint8_t vector)
{
    return register_route(IrqRouteKind::LocalApic, owner, source_id, 0, 0, 0, vector);
}

bool platform_allocate_local_apic_irq_route(DeviceId owner, uint16_t source_id, uint8_t& vector)
{
    vector = 0;
    uint8_t allocated_vector = 0;
    if(!irq_allocate_vector(allocated_vector))
    {
        return false;
    }

    if(!platform_register_local_apic_irq_route(owner, source_id, allocated_vector))
    {
        (void)irq_free_vector(allocated_vector);
        return false;
    }

    vector = allocated_vector;
    return true;
}

bool platform_register_msi_irq_route(DeviceId owner, uint16_t source_id, uint8_t vector)
{
    return register_route(IrqRouteKind::Msi, owner, source_id, 0, 0, 0, vector);
}

bool platform_register_msix_irq_route(DeviceId owner, uint16_t source_id, uint8_t vector)
{
    return register_route(IrqRouteKind::Msix, owner, source_id, 0, 0, 0, vector);
}

const IrqRoute* platform_find_irq_route(uint8_t vector)
{
    for(size_t i = 0; i < g_platform.irq_route_count; ++i)
    {
        const IrqRoute& route = g_platform.irq_routes[i];
        if(route.active && (route.vector == vector))
        {
            return &route;
        }
    }
    return nullptr;
}

bool platform_release_irq_route(uint8_t vector)
{
    for(size_t i = 0; i < g_platform.irq_route_count; ++i)
    {
        IrqRoute& route = g_platform.irq_routes[i];
        if(!route.active || (route.vector != vector))
        {
            continue;
        }

        if(route_kind_uses_dynamic_vector(route.kind) && irq_vector_is_allocated(route.vector))
        {
            (void)irq_free_vector(route.vector);
        }
        route.active = false;
        return true;
    }
    return false;
}

void platform_release_irq_routes_for_owner(DeviceId owner)
{
    for(size_t i = 0; i < g_platform.irq_route_count; ++i)
    {
        IrqRoute& route = g_platform.irq_routes[i];
        if(route.active && device_id_equal(route.owner, owner))
        {
            if(route_kind_uses_dynamic_vector(route.kind) && irq_vector_is_allocated(route.vector))
            {
                (void)irq_free_vector(route.vector);
            }
            route.active = false;
        }
    }
}
