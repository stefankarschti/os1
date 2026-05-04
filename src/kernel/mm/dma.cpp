// DMA buffer ownership and bookkeeping.
#include "mm/dma.hpp"

#include "handoff/memory_layout.h"
#include "mm/kmem.hpp"
#include "platform/state.hpp"
#include "sync/smp.hpp"
#include "util/memory.h"

namespace
{
struct DmaAllocationNode
{
    DmaAllocationRecord record{};
    DmaAllocationNode* next = nullptr;
};

struct DmaAllocationRegistry
{
    Spinlock lock{"dma-allocation-registry"};
    KmemCache* cache = nullptr;
    DmaAllocationNode* head = nullptr;
    DmaAllocationNode* tail = nullptr;
    DmaAllocationRecord* snapshot = nullptr;
    size_t snapshot_capacity = 0;
    size_t count = 0;
};

OS1_BSP_ONLY DmaAllocationRegistry g_dma_allocation_registry{};

class DmaAllocationGuard
{
public:
    explicit DmaAllocationGuard(Spinlock& lock) : irq_guard_(), lock_(lock)
    {
        lock_.lock();
    }

    DmaAllocationGuard(const DmaAllocationGuard&) = delete;
    DmaAllocationGuard& operator=(const DmaAllocationGuard&) = delete;

    ~DmaAllocationGuard()
    {
        lock_.unlock();
    }

private:
    IrqGuard irq_guard_;
    Spinlock& lock_;
};

[[nodiscard]] uint32_t bytes_to_pages(size_t size_bytes)
{
    return static_cast<uint32_t>((size_bytes + kPageSize - 1u) / kPageSize);
}

[[nodiscard]] bool ensure_dma_allocation_cache()
{
    if(nullptr != g_dma_allocation_registry.cache)
    {
        return true;
    }

    g_dma_allocation_registry.cache =
        kmem_cache_create("dma_allocation", sizeof(DmaAllocationNode), alignof(DmaAllocationNode));
    return nullptr != g_dma_allocation_registry.cache;
}

[[nodiscard]] bool ensure_snapshot_capacity(size_t required_capacity)
{
    if(required_capacity <= g_dma_allocation_registry.snapshot_capacity)
    {
        return true;
    }

    size_t new_capacity = (0u == g_dma_allocation_registry.snapshot_capacity)
                              ? 4u
                              : g_dma_allocation_registry.snapshot_capacity;
    while(new_capacity < required_capacity)
    {
        new_capacity *= 2u;
    }

    auto* new_snapshot = static_cast<DmaAllocationRecord*>(kcalloc(new_capacity, sizeof(DmaAllocationRecord)));
    if(nullptr == new_snapshot)
    {
        return false;
    }

    kfree(g_dma_allocation_registry.snapshot);
    g_dma_allocation_registry.snapshot = new_snapshot;
    g_dma_allocation_registry.snapshot_capacity = new_capacity;
    return true;
}

DmaAllocationNode* find_allocation_locked(uint64_t physical_base, DmaAllocationNode** previous = nullptr)
{
    DmaAllocationNode* prior = nullptr;
    for(DmaAllocationNode* node = g_dma_allocation_registry.head; nullptr != node; node = node->next)
    {
        if(node->record.physical_base == physical_base)
        {
            if(nullptr != previous)
            {
                *previous = prior;
            }
            return node;
        }
        prior = node;
    }

    if(nullptr != previous)
    {
        *previous = nullptr;
    }
    return nullptr;
}

void unlink_allocation_locked(DmaAllocationNode* node, DmaAllocationNode* previous)
{
    if(nullptr == previous)
    {
        g_dma_allocation_registry.head = node->next;
    }
    else
    {
        previous->next = node->next;
    }

    if(g_dma_allocation_registry.tail == node)
    {
        g_dma_allocation_registry.tail = previous;
    }

    --g_dma_allocation_registry.count;
}

void free_buffer_pages(PageFrameContainer& frames, uint64_t physical_address, uint32_t page_count)
{
    for(uint32_t page = 0; page < page_count; ++page)
    {
        (void)frames.free(physical_address + static_cast<uint64_t>(page) * kPageSize);
    }
}
}  // namespace

void dma_registry_reset()
{
    if(nullptr != g_dma_allocation_registry.cache)
    {
        for(DmaAllocationNode* node = g_dma_allocation_registry.head; nullptr != node;)
        {
            DmaAllocationNode* next = node->next;
            kmem_cache_free(g_dma_allocation_registry.cache, node);
            node = next;
        }
        (void)kmem_cache_destroy(g_dma_allocation_registry.cache);
    }

    g_dma_allocation_registry.cache = nullptr;
    g_dma_allocation_registry.head = nullptr;
    g_dma_allocation_registry.tail = nullptr;
    g_dma_allocation_registry.count = 0;
    kfree(g_dma_allocation_registry.snapshot);
    g_dma_allocation_registry.snapshot = nullptr;
    g_dma_allocation_registry.snapshot_capacity = 0;
}

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

    {
        DmaAllocationGuard guard(g_dma_allocation_registry.lock);
        if(!ensure_snapshot_capacity(g_dma_allocation_registry.count + 1u) || !ensure_dma_allocation_cache())
        {
            return false;
        }
    }

    const uint32_t page_count = bytes_to_pages(size_bytes);
    uint64_t physical = 0;
    if((0 == page_count) || !frames.allocate(physical, page_count))
    {
        return false;
    }

    void* virtual_address = kernel_physical_pointer<void>(physical);
    memset(virtual_address, 0, static_cast<size_t>(page_count) * kPageSize);

    DmaAllocationGuard guard(g_dma_allocation_registry.lock);
    auto* node = static_cast<DmaAllocationNode*>(
        kmem_cache_alloc(g_dma_allocation_registry.cache, KmallocFlags::Zero));
    if(nullptr == node)
    {
        free_buffer_pages(frames, physical, page_count);
        return false;
    }

    node->record.active = true;
    node->record.owner = owner;
    node->record.direction = direction;
    node->record.coherent = true;
    node->record.page_count = page_count;
    node->record.physical_base = physical;
    node->record.size_bytes = static_cast<uint64_t>(size_bytes);

    if(nullptr == g_dma_allocation_registry.tail)
    {
        g_dma_allocation_registry.head = node;
    }
    else
    {
        g_dma_allocation_registry.tail->next = node;
    }
    g_dma_allocation_registry.tail = node;
    ++g_dma_allocation_registry.count;

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

    {
        DmaAllocationGuard guard(g_dma_allocation_registry.lock);
        DmaAllocationNode* previous = nullptr;
        DmaAllocationNode* node = find_allocation_locked(buffer.physical_address, &previous);
        if(nullptr != node)
        {
            unlink_allocation_locked(node, previous);
            node->record.active = false;
            kmem_cache_free(g_dma_allocation_registry.cache, node);
        }
    }

    free_buffer_pages(frames, buffer.physical_address, buffer.page_count);
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

size_t dma_allocation_count()
{
    DmaAllocationGuard guard(g_dma_allocation_registry.lock);
    return g_dma_allocation_registry.count;
}

const DmaAllocationRecord* dma_allocation_records()
{
    DmaAllocationGuard guard(g_dma_allocation_registry.lock);
    if(0u == g_dma_allocation_registry.count)
    {
        return nullptr;
    }

    size_t index = 0;
    for(DmaAllocationNode* node = g_dma_allocation_registry.head; nullptr != node; node = node->next)
    {
        g_dma_allocation_registry.snapshot[index++] = node->record;
    }
    return g_dma_allocation_registry.snapshot;
}
