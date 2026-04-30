// DMA-safe buffer ownership on top of the page-frame allocator.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.hpp"
#include "platform/types.hpp"

struct DmaBuffer
{
    DeviceId owner{DeviceBus::Platform, 0};
    void* virtual_address = nullptr;
    uint64_t physical_address = 0;
    size_t size_bytes = 0;
    uint32_t page_count = 0;
    DmaDirection direction = DmaDirection::Bidirectional;
    bool coherent = true;
    bool active = false;
};

bool dma_allocate_buffer(PageFrameContainer& frames,
                         DeviceId owner,
                         size_t size_bytes,
                         DmaDirection direction,
                         DmaBuffer& buffer);

void dma_release_buffer(PageFrameContainer& frames, DmaBuffer& buffer);

void dma_sync_for_device(const DmaBuffer& buffer);
void dma_sync_for_cpu(const DmaBuffer& buffer);

size_t dma_allocation_count();
const DmaAllocationRecord* dma_allocation_records();
