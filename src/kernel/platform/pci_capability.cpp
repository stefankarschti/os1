// PCI capability walkers and MSI/MSI-X decoders.
#include "platform/pci_capability.hpp"

#include "platform/pci_config.hpp"

namespace
{
constexpr uint8_t kPciCapabilityMsi = 0x05;
constexpr uint8_t kPciCapabilityMsix = 0x11;

[[nodiscard]] bool capability_offset_is_valid(uint8_t offset)
{
    return (offset >= 0x40u) && (offset <= 0xFCu);
}
}  // namespace

bool pci_find_capability(const PciDevice& device,
                         uint8_t capability_id,
                         PciCapabilityLocation& location)
{
    location = {};

    uint8_t offset = device.capability_pointer;
    for(size_t guard = 0; capability_offset_is_valid(offset) && (guard < 48u); ++guard)
    {
        const uint8_t id = pci_config_read8(device, offset);
        const uint8_t next = pci_config_read8(device, static_cast<uint16_t>(offset + 1u));
        if(id == capability_id)
        {
            location.offset = offset;
            location.id = id;
            location.next = next;
            return true;
        }
        if(0 == next)
        {
            return false;
        }
        offset = next;
    }

    return false;
}

bool pci_parse_msi_capability(const PciDevice& device, PciMsiCapabilityInfo& info)
{
    PciCapabilityLocation location{};
    if(!pci_find_capability(device, kPciCapabilityMsi, location))
    {
        return false;
    }

    info = {};
    info.offset = location.offset;
    info.control = pci_config_read16(device, static_cast<uint16_t>(location.offset + 2u));
    info.is_64_bit = 0 != (info.control & (1u << 7));
    info.per_vector_masking = 0 != (info.control & (1u << 8));
    info.multiple_message_capable = static_cast<uint8_t>((info.control >> 1) & 0x7u);
    return true;
}

bool pci_parse_msix_capability(const PciDevice& device, PciMsixCapabilityInfo& info)
{
    PciCapabilityLocation location{};
    if(!pci_find_capability(device, kPciCapabilityMsix, location))
    {
        return false;
    }

    const uint32_t table = pci_config_read32(device, static_cast<uint16_t>(location.offset + 4u));
    const uint32_t pba = pci_config_read32(device, static_cast<uint16_t>(location.offset + 8u));

    info = {};
    info.offset = location.offset;
    info.control = pci_config_read16(device, static_cast<uint16_t>(location.offset + 2u));
    info.table_bar = static_cast<uint8_t>(table & 0x7u);
    info.pba_bar = static_cast<uint8_t>(pba & 0x7u);
    info.table_offset = table & ~0x7u;
    info.pba_offset = pba & ~0x7u;
    info.table_size = static_cast<uint16_t>((info.control & 0x07FFu) + 1u);
    return true;
}
