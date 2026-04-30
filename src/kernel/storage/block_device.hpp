// Generic block-device request facade above concrete storage drivers.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum class BlockOperation : uint8_t
{
    Read = 0,
    Write = 1,
    Flush = 2,
};

enum class BlockRequestStatus : uint8_t
{
    Pending = 0,
    Success = 1,
    DeviceError = 2,
    Invalid = 3,
    Timeout = 4,
};

struct BlockRequest
{
    BlockOperation operation = BlockOperation::Read;
    uint64_t sector = 0;
    void* buffer = nullptr;
    uint32_t sector_count = 0;
    volatile bool completed = false;
    volatile BlockRequestStatus status = BlockRequestStatus::Pending;
    uint32_t bytes_transferred = 0;
    void* driver_context = nullptr;
};

struct BlockDevice;

using BlockSubmit = bool (*)(BlockDevice& device, BlockRequest& request);
using BlockFlush = bool (*)(BlockDevice& device, BlockRequest& request);

struct BlockDevice
{
    const char* name = nullptr;
    uint64_t sector_count = 0;
    uint32_t sector_size = 0;
    uint32_t max_sectors_per_request = 0;
    uint32_t queue_depth = 0;
    void* driver_state = nullptr;
    BlockSubmit submit = nullptr;
    BlockFlush flush = nullptr;
};

inline bool block_request_succeeded(const BlockRequest& request)
{
    return request.completed && (BlockRequestStatus::Success == request.status);
}

inline bool block_read_sync(BlockDevice& device,
                            uint64_t sector,
                            void* buffer,
                            uint32_t sector_count)
{
    if((nullptr == device.submit) || (nullptr == buffer) || (0 == sector_count))
    {
        return false;
    }

    for(uint32_t i = 0; i < sector_count; ++i)
    {
        BlockRequest request{};
        request.operation = BlockOperation::Read;
        request.sector = sector + i;
        request.buffer = static_cast<uint8_t*>(buffer) +
                         static_cast<size_t>(i) * static_cast<size_t>(device.sector_size);
        request.sector_count = 1;
        if(!device.submit(device, request) || !block_request_succeeded(request))
        {
            return false;
        }
    }
    return true;
}

inline bool block_write_sync(BlockDevice& device,
                             uint64_t sector,
                             const void* buffer,
                             uint32_t sector_count)
{
    if((nullptr == device.submit) || (nullptr == buffer) || (0 == sector_count))
    {
        return false;
    }

    for(uint32_t i = 0; i < sector_count; ++i)
    {
        BlockRequest request{};
        request.operation = BlockOperation::Write;
        request.sector = sector + i;
        request.buffer = const_cast<uint8_t*>(static_cast<const uint8_t*>(buffer) +
                                              static_cast<size_t>(i) * static_cast<size_t>(device.sector_size));
        request.sector_count = 1;
        if(!device.submit(device, request) || !block_request_succeeded(request))
        {
            return false;
        }
    }
    return true;
}
