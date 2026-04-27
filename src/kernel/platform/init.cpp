// Platform initialization sequence: ACPI discovery, topology publication, PCI
// enumeration, and current device probing.
#include "platform/platform.h"

#include "arch/x86_64/apic/ioapic.h"
#include "debug/debug.h"
#include "handoff/bootinfo.h"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.h"
#include "mm/virtual_memory.h"
#include "platform/acpi.h"
#include "platform/device_probe.h"
#include "platform/legacy_mp.h"
#include "platform/pci.h"
#include "platform/state.h"
#include "platform/topology.h"
#include "util/memory.h"

bool platform_init(const BootInfo &boot_info, VirtualMemory &kernel_vm)
{
	memset(&g_platform, 0, sizeof(g_platform));

	const bool acpi_available = DiscoverAcpiPlatform(kernel_vm,
			boot_info,
			g_platform.lapic_base,
			g_platform.cpus,
			g_platform.cpu_count,
			g_platform.ioapics,
			g_platform.ioapic_count,
			g_platform.overrides,
			g_platform.override_count,
			g_platform.ecam_regions,
			g_platform.ecam_region_count);
	if(!acpi_available)
	{
		if(BootSource::Limine == boot_info.source)
		{
			debug("platform: ACPI required on modern boot path")();
			return false;
		}
		return UseLegacyMpFallback(kernel_vm);
	}

	if(!AllocateCpusFromTopology())
	{
		return false;
	}
	if(!MapIdentityRange(kernel_vm, g_platform.lapic_base, kPageSize))
	{
		return false;
	}
	for(size_t i = 0; i < g_platform.ioapic_count; ++i)
	{
		if(!MapIdentityRange(kernel_vm, g_platform.ioapics[i].address, kPageSize))
		{
			return false;
		}
	}

	g_platform.acpi_active = true;
	if(!EnumeratePci(kernel_vm,
			g_platform.ecam_regions,
			g_platform.ecam_region_count,
			g_platform.devices,
			g_platform.device_count)
		|| !ProbeDevices(kernel_vm))
	{
		return false;
	}

	g_platform.initialized = true;
	return true;
}