// Platform discovery sequence: ACPI discovery, topology publication, and PCI
// enumeration. Driver probing is deferred until interrupts are online.
#include "arch/x86_64/apic/ioapic.hpp"
#include "debug/debug.hpp"
#include "handoff/boot_info.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "mm/virtual_memory.hpp"
#include "platform/acpi.hpp"
#include "platform/pci.hpp"
#include "platform/platform.hpp"
#include "platform/state.hpp"
#include "platform/topology.hpp"
#include "sync/smp.hpp"
#include "util/memory.h"

bool platform_discover(const BootInfo& boot_info, VirtualMemory& kernel_vm)
{
    KASSERT_ON_BSP();
    memset(&g_platform, 0, sizeof(g_platform));

    const bool acpi_available = discover_acpi_platform(kernel_vm,
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
        (void)kernel_vm;
        debug("platform: ACPI required; legacy MP discovery removed")();
        return false;
    }

    if(!map_mmio_range(kernel_vm, g_platform.lapic_base, kPageSize))
    {
        return false;
    }
    for(size_t i = 0; i < g_platform.ioapic_count; ++i)
    {
        if(!map_mmio_range(kernel_vm, g_platform.ioapics[i].address, kPageSize))
        {
            return false;
        }
    }
    if(!allocate_cpus_from_topology())
    {
        return false;
    }

    g_platform.acpi_active = true;
    if(!enumerate_pci(kernel_vm,
                      g_platform.ecam_regions,
                      g_platform.ecam_region_count,
                      g_platform.devices,
                      g_platform.device_count))
    {
        return false;
    }

    g_platform.initialized = true;
    return true;
}
