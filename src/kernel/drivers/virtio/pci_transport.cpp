// Shared virtio PCI transport helpers will grow here as more virtio devices
// land. The first live user is still virtio-blk.
#include "drivers/virtio/pci_transport.hpp"

#include "drivers/bus/resource.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "platform/pci_config.hpp"
#include "util/memory.h"

namespace
{
[[nodiscard]] bool map_claimed_bar(VirtualMemory& kernel_vm,
                                   DeviceId owner,
                                   uint16_t pci_index,
                                   uint8_t bar_index,
                                   PciBarResource& resource)
{
    if(!claim_pci_bar(owner, pci_index, bar_index, resource))
    {
        return false;
    }
    if((PciBarType::Mmio32 != resource.type) && (PciBarType::Mmio64 != resource.type))
    {
        return false;
    }
    return map_mmio_range(kernel_vm, resource.base, resource.size);
}
}  // namespace

void virtio_pci_transport_reset(VirtioPciTransport& transport)
{
    memset(&transport, 0, sizeof(transport));
}

void virtio_pci_write_device_status(VirtioPciTransport& transport, uint8_t status)
{
    if(nullptr != transport.common_cfg)
    {
        transport.common_cfg->device_status = status;
    }
}

bool virtio_pci_bind_transport(VirtualMemory& kernel_vm,
                               DeviceId owner,
                               const PciDevice& device,
                               size_t device_cfg_size,
                               VirtioPciTransport& transport)
{
    virtio_pci_transport_reset(transport);

    uint8_t common_bar = 0xFFu;
    uint32_t common_offset = 0;
    uint32_t common_length = 0;
    uint8_t notify_bar = 0xFFu;
    uint32_t notify_offset = 0;
    uint32_t notify_length = 0;
    uint32_t notify_multiplier = 0;
    uint8_t device_bar = 0xFFu;
    uint32_t device_offset = 0;
    uint32_t device_length = 0;
    uint8_t isr_bar = 0xFFu;
    uint32_t isr_offset = 0;
    uint32_t isr_length = 0;

    uint8_t capability = device.capability_pointer;
    for(size_t guard = 0; (0 != capability) && (guard < 48u); ++guard)
    {
        if(capability < 0x40u)
        {
            break;
        }
        const uint8_t cap_id = pci_config_read8(device, capability);
        const uint8_t cap_next = pci_config_read8(device, static_cast<uint16_t>(capability + 1u));
        if(0x09u == cap_id)
        {
            const VirtioPciCapability cap{
                .cap_vndr = cap_id,
                .cap_next = cap_next,
                .cap_len = pci_config_read8(device, static_cast<uint16_t>(capability + 2u)),
                .cfg_type = pci_config_read8(device, static_cast<uint16_t>(capability + 3u)),
                .bar = pci_config_read8(device, static_cast<uint16_t>(capability + 4u)),
                .id = pci_config_read8(device, static_cast<uint16_t>(capability + 5u)),
                .padding = {pci_config_read8(device, static_cast<uint16_t>(capability + 6u)),
                            pci_config_read8(device, static_cast<uint16_t>(capability + 7u))},
                .offset = pci_config_read32(device, static_cast<uint16_t>(capability + 8u)),
                .length = pci_config_read32(device, static_cast<uint16_t>(capability + 12u)),
            };

            switch(cap.cfg_type)
            {
                case kVirtioPciCapCommonCfg:
                    common_bar = cap.bar;
                    common_offset = cap.offset;
                    common_length = cap.length;
                    break;
                case kVirtioPciCapNotifyCfg:
                    notify_bar = cap.bar;
                    notify_offset = cap.offset;
                    notify_length = cap.length;
                    notify_multiplier =
                        pci_config_read32(device, static_cast<uint16_t>(capability + 16u));
                    break;
                case kVirtioPciCapDeviceCfg:
                    device_bar = cap.bar;
                    device_offset = cap.offset;
                    device_length = cap.length;
                    break;
                case kVirtioPciCapIsrCfg:
                    isr_bar = cap.bar;
                    isr_offset = cap.offset;
                    isr_length = cap.length;
                    break;
                default:
                    break;
            }
        }
        if(0 == cap_next)
        {
            break;
        }
        capability = cap_next;
    }

    if((0xFFu == common_bar) || (0xFFu == notify_bar) || (0xFFu == device_bar))
    {
        return false;
    }
    if((common_length < sizeof(VirtioPciCommonCfg)) || (device_length < device_cfg_size) ||
       (0 == notify_multiplier))
    {
        return false;
    }

    PciBarResource common_resource{};
    PciBarResource notify_resource{};
    PciBarResource device_resource{};
    PciBarResource isr_resource{};
    if(!map_claimed_bar(kernel_vm, owner, owner.index, common_bar, common_resource) ||
       !map_claimed_bar(kernel_vm, owner, owner.index, notify_bar, notify_resource) ||
       !map_claimed_bar(kernel_vm, owner, owner.index, device_bar, device_resource))
    {
        return false;
    }
    if((0xFFu != isr_bar) && !map_claimed_bar(kernel_vm, owner, owner.index, isr_bar, isr_resource))
    {
        return false;
    }

    if((common_offset + sizeof(VirtioPciCommonCfg)) > common_resource.size ||
       (notify_offset + notify_length) > notify_resource.size ||
       (device_offset + device_cfg_size) > device_resource.size)
    {
        return false;
    }
    if((0xFFu != isr_bar) && ((isr_offset + 1u) > isr_resource.size) && (0 != isr_length))
    {
        return false;
    }

    pci_enable_mmio_bus_mastering(device);
    transport.owner = owner;
    transport.device = &device;
    transport.common_cfg =
        kernel_physical_pointer<volatile VirtioPciCommonCfg>(common_resource.base + common_offset);
    transport.device_cfg =
        kernel_physical_pointer<volatile uint8_t>(device_resource.base + device_offset);
    transport.notify_bar = notify_bar;
    transport.notify_offset = notify_offset;
    transport.notify_multiplier = notify_multiplier;
    if(0xFFu != isr_bar)
    {
        transport.isr_status = kernel_physical_pointer<volatile uint8_t>(isr_resource.base + isr_offset);
    }
    return true;
}

bool virtio_pci_negotiate_features(VirtioPciTransport& transport,
                                   uint64_t required_features,
                                   uint64_t accepted_features)
{
    if(nullptr == transport.common_cfg)
    {
        return false;
    }

    transport.common_cfg->device_feature_select = 0;
    const uint64_t device_features_low = transport.common_cfg->device_feature;
    transport.common_cfg->device_feature_select = 1;
    const uint64_t device_features_high = transport.common_cfg->device_feature;
    const uint64_t device_features = device_features_low | (device_features_high << 32);
    if((device_features & required_features) != required_features)
    {
        return false;
    }

    const uint64_t negotiated_features = device_features & accepted_features;
    transport.common_cfg->driver_feature_select = 0;
    transport.common_cfg->driver_feature = static_cast<uint32_t>(negotiated_features);
    transport.common_cfg->driver_feature_select = 1;
    transport.common_cfg->driver_feature = static_cast<uint32_t>(negotiated_features >> 32);
    virtio_pci_write_device_status(
        transport,
        static_cast<uint8_t>(transport.common_cfg->device_status | kVirtioStatusFeaturesOk));
    return 0 != (transport.common_cfg->device_status & kVirtioStatusFeaturesOk);
}

bool virtio_pci_setup_queue(VirtioPciTransport& transport,
                            uint16_t queue_index,
                            uint16_t queue_size,
                            uint64_t desc_physical,
                            uint64_t avail_physical,
                            uint64_t used_physical)
{
    if((nullptr == transport.common_cfg) || (0 == queue_size))
    {
        return false;
    }

    transport.common_cfg->queue_select = queue_index;
    const uint16_t device_queue_size = transport.common_cfg->queue_size;
    if((device_queue_size < 3u) || (queue_size > device_queue_size))
    {
        return false;
    }
    transport.queue_size = queue_size;
    transport.common_cfg->queue_size = queue_size;
    transport.common_cfg->queue_desc = desc_physical;
    transport.common_cfg->queue_driver = avail_physical;
    transport.common_cfg->queue_device = used_physical;
    transport.queue_notify_off = transport.common_cfg->queue_notify_off;
    const uint64_t notify_physical = transport.device->bars[transport.notify_bar].base +
                                     transport.notify_offset +
                                     static_cast<uint64_t>(transport.queue_notify_off) * transport.notify_multiplier;
    transport.notify_register = kernel_physical_pointer<volatile uint16_t>(notify_physical);
    transport.common_cfg->queue_enable = 1;
    return true;
}

bool virtio_pci_bind_queue_interrupt(VirtualMemory& kernel_vm,
                                     VirtioPciTransport& transport,
                                     uint16_t queue_index,
                                     uint16_t source_id,
                                     InterruptHandler handler,
                                     void* handler_data)
{
    if((nullptr == transport.common_cfg) || (nullptr == transport.device))
    {
        return false;
    }

    if(!pci_enable_best_interrupt(
           kernel_vm, transport.owner, *transport.device, source_id, handler, handler_data, transport.interrupt))
    {
        transport.common_cfg->msix_config = kVirtioNoVector;
        transport.common_cfg->queue_select = queue_index;
        transport.common_cfg->queue_msix_vector = kVirtioNoVector;
        return false;
    }

    transport.common_cfg->msix_config = kVirtioNoVector;
    transport.common_cfg->queue_select = queue_index;
    if(PciInterruptMode::Msix == transport.interrupt.mode)
    {
        transport.common_cfg->queue_msix_vector = 0;
        if(0 != transport.common_cfg->queue_msix_vector)
        {
            return false;
        }
    }
    else
    {
        transport.common_cfg->queue_msix_vector = kVirtioNoVector;
    }

    return true;
}

void virtio_pci_notify_queue(VirtioPciTransport& transport)
{
    if((nullptr == transport.notify_register) || (nullptr == transport.common_cfg))
    {
        return;
    }
    *transport.notify_register = transport.common_cfg->queue_select;
}
