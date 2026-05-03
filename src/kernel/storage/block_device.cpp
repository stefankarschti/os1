// Generic synchronous block-I/O wrappers and request completion helpers.
#include "storage/block_device.hpp"

#include "proc/thread.hpp"

namespace
{
constexpr uint64_t kInterruptFlag = 1ull << 9;

[[nodiscard]] bool can_sleep_current_thread()
{
    Thread* thread = current_thread();
    return (nullptr != thread) && (nullptr != thread->process);
}

[[nodiscard]] uint64_t read_rflags()
{
    uint64_t flags = 0;
    asm volatile("pushfq; popq %0" : "=r"(flags));
    return flags;
}

void cpu_pause()
{
    asm volatile("pause" : : : "memory");
}

void relax_block_wait()
{
    if(0 != (read_rflags() & kInterruptFlag))
    {
        asm volatile("hlt" : : : "memory");
        return;
    }

    cpu_pause();
}

bool submit_sync_request(BlockDevice& device,
                         BlockOperation operation,
                         uint64_t sector,
                         void* buffer,
                         uint32_t sector_count)
{
    for(;;)
    {
        BlockRequest request{};
        request.operation = operation;
        request.sector = sector;
        request.buffer = buffer;
        request.sector_count = sector_count;
        if(device.submit(device, request))
        {
            return block_request_wait(request) && block_request_succeeded(request);
        }
        if(BlockRequestStatus::Busy != request.status)
        {
            return false;
        }

        relax_block_wait();
    }
}
}  // namespace

void block_request_complete(BlockRequest& request,
                            BlockRequestStatus status,
                            uint32_t bytes_transferred)
{
    request.status = status;
    request.bytes_transferred = bytes_transferred;
    asm volatile("" : : : "memory");
    request.completed = true;
    asm volatile("" : : : "memory");
    wake_block_io_waiters(reinterpret_cast<uint64_t>(&request.completed));
}

bool block_request_wait(BlockRequest& request)
{
    const uint64_t completion_flag = reinterpret_cast<uint64_t>(&request.completed);
    while(!request.completed)
    {
        if(can_sleep_current_thread())
        {
            Thread* thread = current_thread();
            if((ThreadState::Blocked != thread->state) ||
               (ThreadWaitReason::BlockIo != thread->wait.reason) ||
               (completion_flag != thread->wait.block_io.completion_flag))
            {
                block_current_thread_on_block_io(completion_flag);
            }
        }

        if(!request.completed)
        {
            relax_block_wait();
        }
    }

    asm volatile("" : : : "memory");
    return true;
}

namespace
{
[[nodiscard]] uint32_t chunk_sector_count(const BlockDevice& device, uint32_t remaining)
{
    const uint32_t cap = (0u != device.max_sectors_per_request) ? device.max_sectors_per_request : 1u;
    return (remaining < cap) ? remaining : cap;
}
}  // namespace

bool block_read_sync(BlockDevice& device,
                     uint64_t sector,
                     void* buffer,
                     uint32_t sector_count)
{
    if((nullptr == device.submit) || (nullptr == buffer) || (0 == sector_count))
    {
        return false;
    }

    uint32_t remaining = sector_count;
    uint64_t cursor = sector;
    auto* dst = static_cast<uint8_t*>(buffer);
    while(remaining > 0u)
    {
        const uint32_t batch = chunk_sector_count(device, remaining);
        if(!submit_sync_request(device, BlockOperation::Read, cursor, dst, batch))
        {
            return false;
        }
        cursor += batch;
        dst += static_cast<size_t>(batch) * static_cast<size_t>(device.sector_size);
        remaining -= batch;
    }
    return true;
}

bool block_write_sync(BlockDevice& device,
                      uint64_t sector,
                      const void* buffer,
                      uint32_t sector_count)
{
    if((nullptr == device.submit) || (nullptr == buffer) || (0 == sector_count))
    {
        return false;
    }

    uint32_t remaining = sector_count;
    uint64_t cursor = sector;
    const auto* src = static_cast<const uint8_t*>(buffer);
    while(remaining > 0u)
    {
        const uint32_t batch = chunk_sector_count(device, remaining);
        if(!submit_sync_request(device,
                                BlockOperation::Write,
                                cursor,
                                const_cast<uint8_t*>(src),
                                batch))
        {
            return false;
        }
        cursor += batch;
        src += static_cast<size_t>(batch) * static_cast<size_t>(device.sector_size);
        remaining -= batch;
    }
    return true;
}