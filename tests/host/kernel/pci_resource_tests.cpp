#include "drivers/bus/resource.hpp"
#include "platform/state.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace
{
void reset_platform_state()
{
    std::memset(&g_platform, 0, sizeof(g_platform));
    g_platform.device_count = 1;
    g_platform.devices[0].bars[0].base = 0x100000;
    g_platform.devices[0].bars[0].size = 0x2000;
    g_platform.devices[0].bars[0].type = PciBarType::Mmio32;
}
}  // namespace

TEST(PciResource, ClaimIsIdempotentForSameOwner)
{
    reset_platform_state();

    PciBarResource resource{};
    ASSERT_TRUE(claim_pci_bar(DeviceId{DeviceBus::Pci, 0}, 0, 0, resource));
    ASSERT_TRUE(claim_pci_bar(DeviceId{DeviceBus::Pci, 0}, 0, 0, resource));
    EXPECT_EQ(1u, pci_bar_claim_count());
    EXPECT_EQ(0x100000u, resource.base);
}

TEST(PciResource, ClaimRejectsDifferentOwner)
{
    reset_platform_state();

    PciBarResource resource{};
    ASSERT_TRUE(claim_pci_bar(DeviceId{DeviceBus::Pci, 0}, 0, 0, resource));
    EXPECT_FALSE(claim_pci_bar(DeviceId{DeviceBus::Pci, 1}, 0, 0, resource));
}

TEST(PciResource, ReleaseMakesClaimInactive)
{
    reset_platform_state();

    PciBarResource resource{};
    ASSERT_TRUE(claim_pci_bar(DeviceId{DeviceBus::Pci, 0}, 0, 0, resource));
    release_pci_bar(DeviceId{DeviceBus::Pci, 0}, 0, 0);

    const PciBarClaim* claims = pci_bar_claims();
    ASSERT_NE(nullptr, claims);
    EXPECT_FALSE(claims[0].active);
}
