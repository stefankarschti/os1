// Current platform device-probe loop for virtio-blk.
#include "platform/device_probe.h"

#include "debug/debug.h"
#include "drivers/block/virtio_blk.h"
#include "mm/page_frame.h"
#include "mm/virtual_memory.h"
#include "platform/state.h"
#include "storage/block_device.h"
#include "util/memory.h"

extern PageFrameContainer page_frames;

bool ProbeDevices(VirtualMemory &kernel_vm)
{
	memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
	for(size_t i = 0; i < g_platform.device_count; ++i)
	{
		if(!ProbeVirtioBlkDevice(kernel_vm, page_frames, g_platform.devices[i], i, g_platform.virtio_blk_public))
		{
			return false;
		}
	}
	if(!g_platform.virtio_blk_public.present)
	{
		debug("virtio-blk: no device present")();
	}
	g_platform.block_device = VirtioBlkBlockDevice();
	return RunVirtioBlkSmoke();
}