// PCI ECAM enumeration. The platform layer provides ACPI MCFG windows; this
// module walks functions, sizes BARs, and returns normalized PciDevice records.
#pragma once

#include <stddef.h>

#include "platform/platform.hpp"

class VirtualMemory;

// Enumerate all PCI functions reachable through the supplied ECAM regions.
bool enumerate_pci(VirtualMemory &kernel_vm,
		const PciEcamRegion *regions,
		size_t region_count,
		PciDevice *devices,
		size_t &device_count);

