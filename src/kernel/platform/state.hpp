// Mutable platform-wide discovery state. This remains centralized so ACPI, PCI,
// IRQ routing, and driver probing share one normalized view of the machine.
#pragma once

#include "platform/acpi_aml.hpp"
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
    size_t irq_route_count;
    IrqRoute irq_routes[kPlatformMaxIrqRoutes];
    size_t device_binding_count;
    DeviceBinding device_bindings[kPlatformMaxDeviceBindings];
    size_t bar_claim_count;
    PciBarClaim bar_claims[kPlatformMaxPciBarClaims];
    size_t dma_allocation_count;
    DmaAllocationRecord dma_allocations[kPlatformMaxDmaAllocations];
    HpetInfo hpet;
    AcpiFixedInfo acpi_fixed;
    size_t acpi_definition_block_count;
    AcpiDefinitionBlock acpi_definition_blocks[kPlatformMaxAcpiDefinitionBlocks];
    size_t acpi_device_count;
    AcpiDeviceInfo acpi_devices[kAcpiMaxDevices];
    size_t acpi_pci_route_count;
    AcpiPciRoute acpi_pci_routes[kAcpiMaxPciRoutes];
    const BlockDevice* block_device;
    VirtioBlkDevice virtio_blk_public;
};

// BSP-only for now: single normalized platform state instance built during
// platform discovery and later augmented by device probing.
OS1_BSP_ONLY extern PlatformState g_platform;
