// Public platform facade. The implementation is split by responsibility under
// platform/, but callers should use this narrow API for bring-up, IRQ routing,
// and read-only device/topology observation.
#pragma once

#include <stddef.h>

#include "platform/types.hpp"

class VirtualMemory;
struct BlockDevice;
struct BootInfo;

// Discover machine topology, map interrupt controllers, enumerate PCI, and run
// the current virtio-blk probe path.
bool platform_init(const BootInfo& boot_info, VirtualMemory& kernel_vm);

// Route an ISA IRQ through the discovered IOAPIC override table.
bool platform_enable_isa_irq(int bus_irq, int irq = -1);

// Return the currently selected generic block device, if any.
const BlockDevice* platform_block_device();

// Return the public virtio-blk summary when a virtio block device is present.
const VirtioBlkDevice* platform_virtio_blk();

// Return the number of PCI functions found during ECAM enumeration.
size_t platform_pci_device_count();

// Return the fixed PCI device table owned by platform state.
const PciDevice* platform_pci_devices();
