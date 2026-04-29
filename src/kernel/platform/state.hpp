// Mutable platform-wide discovery state. This remains centralized so ACPI, PCI,
// IRQ routing, and driver probing share one normalized view of the machine.
#pragma once

#include "platform/types.hpp"
#include "sync/smp.hpp"

struct BlockDevice;

struct PlatformState
{
    bool initialized;
    bool acpi_active;
    uint64_t lapic_base;
    size_t cpu_count;
    CpuInfo cpus[kPlatformMaxCpus];
    size_t ioapic_count;
    IoApicInfo ioapics[kPlatformMaxIoApics];
    size_t override_count;
    InterruptOverride overrides[kPlatformMaxInterruptOverrides];
    size_t ecam_region_count;
    PciEcamRegion ecam_regions[kPlatformMaxPciEcamRegions];
    // BSP-only for now: PCI enumeration publishes this list once during boot.
    size_t device_count;
    PciDevice devices[kPlatformMaxPciDevices];
    const BlockDevice* block_device;
    VirtioBlkDevice virtio_blk_public;
};

// BSP-only for now: single normalized platform state instance built during
// platform discovery and later augmented by device probing.
OS1_BSP_ONLY extern PlatformState g_platform;
