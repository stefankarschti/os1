// Platform-state storage and read-only public accessors.
#include "platform/state.hpp"

#include "storage/block_device.hpp"
#include "util/memory.h"

void device_binding_registry_reset();
void pci_bar_claim_registry_reset();
void platform_irq_registry_reset();
void dma_registry_reset();

// BSP-only for now: PCI/device discovery state is populated on the BSP during
// boot and exposed through read-only accessors.
OS1_BSP_ONLY constinit PlatformState g_platform{};

void platform_reset_driver_state()
{
    device_binding_registry_reset();
    pci_bar_claim_registry_reset();
    platform_irq_registry_reset();
    dma_registry_reset();
    g_platform.block_device = nullptr;
    memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
}

void platform_reset_state()
{
    platform_reset_driver_state();
    memset(&g_platform, 0, sizeof(g_platform));
}

const BlockDevice* platform_block_device()
{
    return g_platform.block_device;
}

const VirtioBlkDevice* platform_virtio_blk()
{
    return g_platform.virtio_blk_public.present ? &g_platform.virtio_blk_public : nullptr;
}

size_t platform_pci_device_count()
{
    return g_platform.device_count;
}

const PciDevice* platform_pci_devices()
{
    return g_platform.devices;
}

size_t platform_acpi_device_count()
{
    return g_platform.acpi_device_count;
}

const AcpiDeviceInfo* platform_acpi_devices()
{
    return g_platform.acpi_devices;
}
