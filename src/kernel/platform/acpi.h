#ifndef OS1_KERNEL_PLATFORM_ACPI_H
#define OS1_KERNEL_PLATFORM_ACPI_H

#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"
#include "platform.h"

class VirtualMemory;

bool DiscoverAcpiPlatform(VirtualMemory &kernel_vm,
		const BootInfo &boot_info,
		uint64_t &lapic_base,
		CpuInfo *cpus,
		size_t &cpu_count,
		IoApicInfo *ioapics,
		size_t &ioapic_count,
		InterruptOverride *overrides,
		size_t &override_count,
		PciEcamRegion *ecam_regions,
		size_t &ecam_region_count);

#endif
