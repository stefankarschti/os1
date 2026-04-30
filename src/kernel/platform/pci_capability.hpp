// Shared PCI capability walkers and decoded MSI/MSI-X metadata.
#pragma once

#include <stdint.h>

#include "platform/types.hpp"

struct PciCapabilityLocation
{
    uint8_t offset = 0;
    uint8_t id = 0;
    uint8_t next = 0;
};

bool pci_find_capability(const PciDevice& device,
                         uint8_t capability_id,
                         PciCapabilityLocation& location);

struct PciMsiCapabilityInfo
{
    uint8_t offset = 0;
    uint16_t control = 0;
    bool is_64_bit = false;
    bool per_vector_masking = false;
    uint8_t multiple_message_capable = 0;
};

struct PciMsixCapabilityInfo
{
    uint8_t offset = 0;
    uint16_t control = 0;
    uint8_t table_bar = 0;
    uint8_t pba_bar = 0;
    uint32_t table_offset = 0;
    uint32_t pba_offset = 0;
    uint16_t table_size = 0;
};

bool pci_parse_msi_capability(const PciDevice& device, PciMsiCapabilityInfo& info);
bool pci_parse_msix_capability(const PciDevice& device, PciMsixCapabilityInfo& info);
