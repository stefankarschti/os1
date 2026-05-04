#include "arch/x86_64/interrupt/vector_allocator.hpp"
#include "platform/irq_registry.hpp"
#include "platform/platform.hpp"
#include "platform/state.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace
{
void reset_state()
{
    std::memset(&g_platform, 0, sizeof(g_platform));
    irq_vector_allocator_reset();
}
}  // namespace

TEST(IrqRouteRegistry, RegistersLegacyRoutesByVector)
{
    reset_state();

    ASSERT_TRUE(platform_register_isa_irq_route(
        DeviceId{DeviceBus::Platform, 0}, IRQ_TIMER, IRQ_TIMER, 0, static_cast<uint8_t>(T_IRQ0 + IRQ_TIMER)));
    ASSERT_EQ(1u, platform_irq_route_count());

    const IrqRoute* route = platform_find_irq_route(static_cast<uint8_t>(T_IRQ0 + IRQ_TIMER));
    ASSERT_NE(nullptr, route);
    EXPECT_EQ(IrqRouteKind::LegacyIsa, route->kind);
    EXPECT_EQ(DeviceBus::Platform, route->owner.bus);
    EXPECT_EQ(IRQ_TIMER, route->source_irq);
    EXPECT_EQ(IRQ_TIMER, route->gsi);
}

TEST(IrqRouteRegistry, ReleasingDynamicRouteReturnsVectorToAllocator)
{
    reset_state();

    uint8_t vector = 0;
    ASSERT_TRUE(irq_allocate_vector(vector));
    ASSERT_TRUE(platform_register_msix_irq_route(DeviceId{DeviceBus::Pci, 7}, 0, vector));
    EXPECT_TRUE(irq_vector_is_allocated(vector));

    ASSERT_TRUE(platform_release_irq_route(vector));
    EXPECT_FALSE(irq_vector_is_allocated(vector));
    EXPECT_EQ(nullptr, platform_find_irq_route(vector));
}

TEST(IrqRouteRegistry, AllocatesAndReleasesLocalApicRoutes)
{
    reset_state();

    uint8_t vector = 0;
    ASSERT_TRUE(
        platform_allocate_local_apic_irq_route(DeviceId{DeviceBus::Platform, 2}, T_LTIMER, vector));
    EXPECT_TRUE(irq_vector_is_allocated(vector));

    const IrqRoute* route = platform_find_irq_route(vector);
    ASSERT_NE(nullptr, route);
    EXPECT_EQ(IrqRouteKind::LocalApic, route->kind);
    EXPECT_EQ(DeviceBus::Platform, route->owner.bus);
    EXPECT_EQ(static_cast<uint16_t>(T_LTIMER), route->source_id);

    ASSERT_TRUE(platform_release_irq_route(vector));
    EXPECT_FALSE(irq_vector_is_allocated(vector));
    EXPECT_EQ(nullptr, platform_find_irq_route(vector));
}

TEST(IrqRouteRegistry, DuplicateVectorsAreRejected)
{
    reset_state();

    const uint8_t vector = static_cast<uint8_t>(T_IRQ0 + IRQ_KBD);
    ASSERT_TRUE(platform_register_isa_irq_route(DeviceId{DeviceBus::Platform, 1}, IRQ_KBD, IRQ_KBD, 0, vector));
    EXPECT_FALSE(platform_register_msi_irq_route(DeviceId{DeviceBus::Pci, 1}, 0, vector));
}
