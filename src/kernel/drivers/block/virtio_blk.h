#ifndef OS1_KERNEL_DRIVERS_BLOCK_VIRTIO_BLK_H
#define OS1_KERNEL_DRIVERS_BLOCK_VIRTIO_BLK_H

#include <stddef.h>

#include "pageframe.h"
#include "platform.h"

struct BlockDevice;

class VirtualMemory;

bool ProbeVirtioBlkDevice(VirtualMemory &kernel_vm,
		PageFrameContainer &frames,
		const PciDevice &device,
		size_t device_index,
		VirtioBlkDevice &public_device);
bool RunVirtioBlkSmoke();
const BlockDevice *VirtioBlkBlockDevice();

#endif
