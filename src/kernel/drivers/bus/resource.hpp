// Resource ownership helpers for PCI BAR claims.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "platform/types.hpp"

struct PciBarResource
{
    uint16_t pci_index = 0;
    uint8_t bar_index = 0;
    PciBarType type = PciBarType::Unused;
    uint64_t base = 0;
    uint64_t size = 0;
};

bool claim_pci_bar(DeviceId owner, uint16_t pci_index, uint8_t bar_index, PciBarResource& resource);
void release_pci_bar(DeviceId owner, uint16_t pci_index, uint8_t bar_index);
void release_pci_bars_for_owner(DeviceId owner);
size_t pci_bar_claim_count();
const PciBarClaim* pci_bar_claims();
