// Shared virtqueue bookkeeping.
#pragma once

#include <stdint.h>

#include "mm/dma.hpp"
#include "mm/page_frame.hpp"

constexpr uint16_t kVirtqueueMaxSize = 8;
constexpr uint16_t kVirtqDescFlagNext = 1u << 0;
constexpr uint16_t kVirtqDescFlagWrite = 1u << 1;

struct [[gnu::packed]] VirtqDesc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct [[gnu::packed]] VirtqUsedElem
{
    uint32_t id;
    uint32_t len;
};

struct [[gnu::packed]] VirtqAvail
{
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[kVirtqueueMaxSize];
    uint16_t used_event;
};

struct [[gnu::packed]] VirtqUsed
{
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[kVirtqueueMaxSize];
    uint16_t avail_event;
};

struct Virtqueue
{
    uint16_t size = 0;
    uint16_t last_used_idx = 0;
    DmaBuffer ring_memory{};
    VirtqDesc* desc = nullptr;
    VirtqAvail* avail = nullptr;
    volatile VirtqUsed* used = nullptr;
};

void virtqueue_reset(Virtqueue& queue);
bool virtqueue_allocate(PageFrameContainer& frames, DeviceId owner, uint16_t size, Virtqueue& queue);
void virtqueue_release(PageFrameContainer& frames, Virtqueue& queue);
bool virtqueue_submit(Virtqueue& queue, uint16_t head_descriptor);
bool virtqueue_consume_used(Virtqueue& queue, VirtqUsedElem& element);
