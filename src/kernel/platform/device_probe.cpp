// Current platform device-probe loop for virtio-blk.
#include "platform/device_probe.hpp"

#include "debug/debug.hpp"
#include "drivers/block/virtio_blk.hpp"
#include "mm/page_frame.hpp"
#include "mm/virtual_memory.hpp"
#include "platform/state.hpp"
#include "storage/block_device.hpp"
#include "util/memory.h"

extern PageFrameContainer page_frames;

bool probe_devices(VirtualMemory &kernel_vm)
{
	memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
	for(size_t i = 0; i < g_platform.device_count; ++i)
	{
		if(!probe_virtio_blk_device(kernel_vm, page_frames, g_platform.devices[i], i, g_platform.virtio_blk_public))
		{
			return false;
		}
	}
	if(!g_platform.virtio_blk_public.present)
	{
		debug("virtio-blk: no device present")();
	}
	g_platform.block_device = VirtioBlkBlockDevice();
	return run_virtio_blk_smoke();
}