// Shared PCI config-space helpers.
#include "platform/pci_config.hpp"

#include "handoff/memory_layout.h"

uint8_t pci_config_read8(uint64_t config_physical, uint16_t offset)
{
    return *kernel_physical_pointer<volatile uint8_t>(config_physical + offset);
}

uint16_t pci_config_read16(uint64_t config_physical, uint16_t offset)
{
    return *kernel_physical_pointer<volatile uint16_t>(config_physical + offset);
}

uint32_t pci_config_read32(uint64_t config_physical, uint16_t offset)
{
    return *kernel_physical_pointer<volatile uint32_t>(config_physical + offset);
}

void pci_config_write16(uint64_t config_physical, uint16_t offset, uint16_t value)
{
    *kernel_physical_pointer<volatile uint16_t>(config_physical + offset) = value;
}

void pci_config_write32(uint64_t config_physical, uint16_t offset, uint32_t value)
{
    *kernel_physical_pointer<volatile uint32_t>(config_physical + offset) = value;
}

uint8_t pci_header_type_kind(uint8_t header_type)
{
    return header_type & 0x7Fu;
}

uint64_t pci_bdf(const PciDevice& device)
{
    return (static_cast<uint64_t>(device.segment_group) << 16) |
           (static_cast<uint64_t>(device.bus) << 8) |
           (static_cast<uint64_t>(device.slot) << 3) | static_cast<uint64_t>(device.function);
}

uint16_t pci_read_command(const PciDevice& device)
{
    return pci_config_read16(device, 0x04);
}

void pci_write_command(const PciDevice& device, uint16_t command)
{
    pci_config_write16(device, 0x04, command);
}

void pci_set_command_bits(const PciDevice& device, uint16_t set_bits, uint16_t clear_bits)
{
    uint16_t command = pci_read_command(device);
    command = static_cast<uint16_t>((command | set_bits) & ~clear_bits);
    pci_write_command(device, command);
}

void pci_enable_mmio_bus_mastering(const PciDevice& device)
{
    pci_set_command_bits(device, 0x0002u | 0x0004u);
}

void pci_disable_intx(const PciDevice& device, bool disabled)
{
    constexpr uint16_t kPciCommandInterruptDisable = 1u << 10;
    pci_set_command_bits(
        device, disabled ? kPciCommandInterruptDisable : 0u, disabled ? 0u : kPciCommandInterruptDisable);
}
