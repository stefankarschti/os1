#ifndef OS1_KERNEL_PLATFORM_PCI_H
#define OS1_KERNEL_PLATFORM_PCI_H

#include <stddef.h>

#include "platform.h"

class VirtualMemory;

bool EnumeratePci(VirtualMemory &kernel_vm,
		const PciEcamRegion *regions,
		size_t region_count,
		PciDevice *devices,
		size_t &device_count);

#endif
