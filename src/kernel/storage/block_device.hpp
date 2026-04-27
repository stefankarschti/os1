// Generic block-device facade above concrete storage drivers. Drivers fill one
// of these records; storage and filesystem layers should depend on this shape,
// not on virtio-specific queue state.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct BlockDevice;

using BlockRead = bool (*)(BlockDevice& device, uint64_t sector, void* buffer, size_t sector_count);
using BlockWrite = bool (*)(BlockDevice& device,
                            uint64_t sector,
                            const void* buffer,
                            size_t sector_count);

// Fixed block-device descriptor with sector geometry and driver callbacks.
struct BlockDevice
{
    const char* name = nullptr;
    uint64_t sector_count = 0;
    uint32_t sector_size = 0;
    void* driver_state = nullptr;
    BlockRead read = nullptr;
    BlockWrite write = nullptr;
};
