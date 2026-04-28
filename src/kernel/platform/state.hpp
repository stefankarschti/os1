// Mutable platform-wide discovery state. This remains centralized so ACPI, PCI,
// IRQ routing, and driver probing share one normalized view of the machine.
#pragma once

#include "platform/types.hpp"

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
    size_t device_count;
    PciDevice devices[kPlatformMaxPciDevices];
    const BlockDevice* block_device;
    VirtioBlkDevice virtio_blk_public;
};

// Single normalized platform state instance built by platform_init.
extern PlatformState g_platform;
