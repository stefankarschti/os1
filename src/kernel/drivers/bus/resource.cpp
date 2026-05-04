// PCI BAR ownership records shared by bus and driver code.
#include "drivers/bus/resource.hpp"

#include "debug/debug.hpp"
#include "platform/state.hpp"

namespace
{
[[nodiscard]] bool device_id_equal(DeviceId left, DeviceId right)
{
    return (left.bus == right.bus) && (left.index == right.index);
}

PciBarClaim* find_claim(uint16_t pci_index, uint8_t bar_index)
{
    for(size_t i = 0; i < g_platform.bar_claim_count; ++i)
    {
        PciBarClaim& claim = g_platform.bar_claims[i];
        if(claim.active && (claim.pci_index == pci_index) && (claim.bar_index == bar_index))
        {
            return &claim;
        }
    }
    return nullptr;
}
}  // namespace

bool claim_pci_bar(DeviceId owner, uint16_t pci_index, uint8_t bar_index, PciBarResource& resource)
{
    resource = {};
    if((pci_index >= g_platform.device_count) || (bar_index >= 6u))
    {
        return false;
    }

    const PciDevice& device = g_platform.devices[pci_index];
    const PciBarInfo& bar = device.bars[bar_index];
    if((0 == bar.base) || (0 == bar.size) || (PciBarType::Unused == bar.type))
    {
        return false;
    }

    if(PciBarClaim* existing = find_claim(pci_index, bar_index))
    {
        if(!device_id_equal(existing->owner, owner))
        {
            debug("pci: BAR already claimed pci=")(pci_index)(" bar=")(bar_index)();
            return false;
        }

        resource.pci_index = pci_index;
        resource.bar_index = bar_index;
        resource.type = existing->type;
        resource.base = existing->base;
        resource.size = existing->size;
        return true;
    }

    if(g_platform.bar_claim_count >= kPlatformMaxPciBarClaims)
    {
        debug("pci: BAR claim table full")();
        return false;
    }

    PciBarClaim& claim = g_platform.bar_claims[g_platform.bar_claim_count++];
    claim.active = true;
    claim.owner = owner;
    claim.pci_index = pci_index;
    claim.bar_index = bar_index;
    claim.type = bar.type;
    claim.base = bar.base;
    claim.size = bar.size;

    resource.pci_index = pci_index;
    resource.bar_index = bar_index;
    resource.type = bar.type;
    resource.base = bar.base;
    resource.size = bar.size;
    return true;
}

void release_pci_bar(DeviceId owner, uint16_t pci_index, uint8_t bar_index)
{
    PciBarClaim* claim = find_claim(pci_index, bar_index);
    if((nullptr != claim) && device_id_equal(claim->owner, owner))
    {
        claim->active = false;
    }
}

void release_pci_bars_for_owner(DeviceId owner)
{
    for(size_t i = 0; i < g_platform.bar_claim_count; ++i)
    {
        PciBarClaim& claim = g_platform.bar_claims[i];
        if(claim.active && device_id_equal(claim.owner, owner))
        {
            claim.active = false;
        }
    }
}

size_t pci_bar_claim_count()
{
    return g_platform.bar_claim_count;
}

const PciBarClaim* pci_bar_claims()
{
    return g_platform.bar_claims;
}
