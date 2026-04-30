// Shared virtqueue bookkeeping.
#include "drivers/virtio/virtqueue.hpp"

#include "handoff/memory_layout.h"
#include "util/memory.h"

void virtqueue_reset(Virtqueue& queue)
{
    memset(&queue, 0, sizeof(queue));
}

bool virtqueue_allocate(PageFrameContainer& frames, DeviceId owner, uint16_t size, Virtqueue& queue)
{
    virtqueue_reset(queue);
    if((0 == size) || (size > kVirtqueueMaxSize))
    {
        return false;
    }
    if(!dma_allocate_buffer(frames, owner, 3u * kPageSize, DmaDirection::Bidirectional, queue.ring_memory))
    {
        return false;
    }

    queue.size = size;
    queue.desc = static_cast<VirtqDesc*>(queue.ring_memory.virtual_address);
    queue.avail = reinterpret_cast<VirtqAvail*>(
        static_cast<uint8_t*>(queue.ring_memory.virtual_address) + kPageSize);
    queue.used = reinterpret_cast<volatile VirtqUsed*>(
        static_cast<uint8_t*>(queue.ring_memory.virtual_address) + 2u * kPageSize);
    queue.last_used_idx = queue.used->idx;
    return true;
}

void virtqueue_release(PageFrameContainer& frames, Virtqueue& queue)
{
    dma_release_buffer(frames, queue.ring_memory);
    virtqueue_reset(queue);
}

bool virtqueue_submit(Virtqueue& queue, uint16_t head_descriptor)
{
    if((nullptr == queue.avail) || (head_descriptor >= queue.size))
    {
        return false;
    }

    const uint16_t slot = static_cast<uint16_t>(queue.avail->idx % queue.size);
    queue.avail->ring[slot] = head_descriptor;
    asm volatile("" : : : "memory");
    ++queue.avail->idx;
    asm volatile("" : : : "memory");
    return true;
}

bool virtqueue_consume_used(Virtqueue& queue, VirtqUsedElem& element)
{
    if((nullptr == queue.used) || (queue.used->idx == queue.last_used_idx))
    {
        return false;
    }

    const uint16_t slot = static_cast<uint16_t>(queue.last_used_idx % queue.size);
    element.id = queue.used->ring[slot].id;
    element.len = queue.used->ring[slot].len;
    queue.last_used_idx = static_cast<uint16_t>(queue.last_used_idx + 1u);
    return true;
}
