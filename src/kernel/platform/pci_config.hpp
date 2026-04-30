// Shared PCI config-space access helpers used by platform enumeration and
// driver code.
#pragma once

#include <stdint.h>

#include "platform/types.hpp"

[[nodiscard]] uint8_t pci_config_read8(uint64_t config_physical, uint16_t offset);
[[nodiscard]] uint16_t pci_config_read16(uint64_t config_physical, uint16_t offset);
[[nodiscard]] uint32_t pci_config_read32(uint64_t config_physical, uint16_t offset);

void pci_config_write16(uint64_t config_physical, uint16_t offset, uint16_t value);
void pci_config_write32(uint64_t config_physical, uint16_t offset, uint32_t value);

[[nodiscard]] inline uint8_t pci_config_read8(const PciDevice& device, uint16_t offset)
{
    return pci_config_read8(device.config_physical, offset);
}

[[nodiscard]] inline uint16_t pci_config_read16(const PciDevice& device, uint16_t offset)
{
    return pci_config_read16(device.config_physical, offset);
}

[[nodiscard]] inline uint32_t pci_config_read32(const PciDevice& device, uint16_t offset)
{
    return pci_config_read32(device.config_physical, offset);
}

inline void pci_config_write16(const PciDevice& device, uint16_t offset, uint16_t value)
{
    pci_config_write16(device.config_physical, offset, value);
}

inline void pci_config_write32(const PciDevice& device, uint16_t offset, uint32_t value)
{
    pci_config_write32(device.config_physical, offset, value);
}

[[nodiscard]] uint8_t pci_header_type_kind(uint8_t header_type);
[[nodiscard]] uint64_t pci_bdf(const PciDevice& device);

[[nodiscard]] uint16_t pci_read_command(const PciDevice& device);
void pci_write_command(const PciDevice& device, uint16_t command);
void pci_set_command_bits(const PciDevice& device, uint16_t set_bits, uint16_t clear_bits = 0);
void pci_enable_mmio_bus_mastering(const PciDevice& device);
void pci_disable_intx(const PciDevice& device, bool disabled);
