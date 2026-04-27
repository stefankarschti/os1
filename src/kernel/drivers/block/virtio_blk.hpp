// Virtio block driver. Platform discovery passes candidate PCI functions here;
// the driver owns queue setup, feature negotiation, smoke I/O, and publication
// of a generic BlockDevice facade.
#pragma once

#include <stddef.h>

#include "mm/page_frame.hpp"
#include "platform/platform.hpp"

struct BlockDevice;

class VirtualMemory;

// Try to bind one PCI function as virtio-blk and publish public device metadata.
bool probe_virtio_blk_device(VirtualMemory &kernel_vm,
		PageFrameContainer &frames,
		const PciDevice &device,
		size_t device_index,
		VirtioBlkDevice &public_device);
// Run the current read-only sector smoke check against the bound device.
bool run_virtio_blk_smoke();
// Return the generic block facade for filesystem/storage code.
const BlockDevice *VirtioBlkBlockDevice();

