// DMA buffer ownership and bookkeeping.
#include "mm/dma.hpp"

#include "handoff/memory_layout.h"
#include "platform/state.hpp"
#include "util/memory.h"

namespace
{
[[nodiscard]] uint32_t bytes_to_pages(size_t size_bytes)
{
    return static_cast<uint32_t>((size_bytes + kPageSize - 1u) / kPageSize);
}
}  // namespace

bool dma_allocate_buffer(PageFrameContainer& frames,
                         DeviceId owner,
                         size_t size_bytes,
                         DmaDirection direction,
                         DmaBuffer& buffer)
{
    buffer = {};
    if(0 == size_bytes)
    {
        return false;
    }
    if(g_platform.dma_allocation_count >= kPlatformMaxDmaAllocations)
    {
        return false;
    }

    const uint32_t page_count = bytes_to_pages(size_bytes);
    uint64_t physical = 0;
    if((0 == page_count) || !frames.allocate(physical, page_count))
    {
        return false;
    }

    void* virtual_address = kernel_physical_pointer<void>(physical);
    memset(virtual_address, 0, static_cast<size_t>(page_count) * kPageSize);

    DmaAllocationRecord& record = g_platform.dma_allocations[g_platform.dma_allocation_count++];
    record.active = true;
    record.owner = owner;
    record.direction = direction;
    record.coherent = true;
    record.page_count = page_count;
    record.physical_base = physical;
    record.size_bytes = static_cast<uint64_t>(size_bytes);

    buffer.owner = owner;
    buffer.virtual_address = virtual_address;
    buffer.physical_address = physical;
    buffer.size_bytes = size_bytes;
    buffer.page_count = page_count;
    buffer.direction = direction;
    buffer.coherent = true;
    buffer.active = true;
    return true;
}

void dma_release_buffer(PageFrameContainer& frames, DmaBuffer& buffer)
{
    if(!buffer.active)
    {
        return;
    }

    for(size_t i = 0; i < g_platform.dma_allocation_count; ++i)
    {
        DmaAllocationRecord& record = g_platform.dma_allocations[i];
        if(record.active && (record.physical_base == buffer.physical_address))
        {
            record.active = false;
            break;
        }
    }

    for(uint32_t page = 0; page < buffer.page_count; ++page)
    {
        (void)frames.free(buffer.physical_address + static_cast<uint64_t>(page) * kPageSize);
    }

    buffer = {};
}

void dma_sync_for_device(const DmaBuffer& buffer)
{
    (void)buffer;
    asm volatile("" : : : "memory");
}

void dma_sync_for_cpu(const DmaBuffer& buffer)
{
    (void)buffer;
    asm volatile("" : : : "memory");
}
