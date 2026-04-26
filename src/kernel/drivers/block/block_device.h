#ifndef OS1_KERNEL_DRIVERS_BLOCK_BLOCK_DEVICE_H
#define OS1_KERNEL_DRIVERS_BLOCK_BLOCK_DEVICE_H

#include <stddef.h>
#include <stdint.h>

struct BlockDevice;

using BlockRead = bool (*)(BlockDevice &device, uint64_t sector, void *buffer, size_t sector_count);
using BlockWrite = bool (*)(BlockDevice &device, uint64_t sector, const void *buffer, size_t sector_count);

struct BlockDevice
{
	const char *name = nullptr;
	uint64_t sector_count = 0;
	uint32_t sector_size = 0;
	void *driver_state = nullptr;
	BlockRead read = nullptr;
	BlockWrite write = nullptr;
};

#endif
