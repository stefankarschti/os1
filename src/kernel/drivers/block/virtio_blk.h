// Virtio block driver. Platform discovery passes candidate PCI functions here;
// the driver owns queue setup, feature negotiation, smoke I/O, and publication
// of a generic BlockDevice facade.
#ifndef OS1_KERNEL_DRIVERS_BLOCK_VIRTIO_BLK_H
#define OS1_KERNEL_DRIVERS_BLOCK_VIRTIO_BLK_H

#include <stddef.h>

#include "mm/page_frame.h"
#include "platform/platform.h"

struct BlockDevice;

class VirtualMemory;

// Try to bind one PCI function as virtio-blk and publish public device metadata.
bool ProbeVirtioBlkDevice(VirtualMemory &kernel_vm,
		PageFrameContainer &frames,
		const PciDevice &device,
		size_t device_index,
		VirtioBlkDevice &public_device);
// Run the current read-only sector smoke check against the bound device.
bool RunVirtioBlkSmoke();
// Return the generic block facade for filesystem/storage code.
const BlockDevice *VirtioBlkBlockDevice();

#endif
