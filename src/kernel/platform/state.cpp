// Platform-state storage and read-only public accessors.
#include "platform/state.h"

#include "storage/block_device.h"

constinit PlatformState g_platform{};

const BlockDevice *platform_block_device()
{
	return g_platform.block_device;
}

const VirtioBlkDevice *platform_virtio_blk()
{
	return g_platform.virtio_blk_public.present ? &g_platform.virtio_blk_public : nullptr;
}

size_t platform_pci_device_count()
{
	return g_platform.device_count;
}

const PciDevice *platform_pci_devices()
{
	return g_platform.devices;
}