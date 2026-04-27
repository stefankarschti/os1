// PCI ECAM enumeration. The platform layer provides ACPI MCFG windows; this
// module walks functions, sizes BARs, and returns normalized PciDevice records.
#ifndef OS1_KERNEL_PLATFORM_PCI_H
#define OS1_KERNEL_PLATFORM_PCI_H

#include <stddef.h>

#include "platform/platform.h"

class VirtualMemory;

// Enumerate all PCI functions reachable through the supplied ECAM regions.
bool EnumeratePci(VirtualMemory &kernel_vm,
		const PciEcamRegion *regions,
		size_t region_count,
		PciDevice *devices,
		size_t &device_count);

#endif // OS1_KERNEL_PLATFORM_PCI_H
