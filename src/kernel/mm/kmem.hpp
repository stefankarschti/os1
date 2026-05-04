// Direct-map-backed kernel small-object allocator.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.hpp"

enum class KmallocFlags : uint32_t
{
    None = 0,
    Zero = 1u << 0,
    NoGrow = 1u << 1,
    Atomic = 1u << 2,
    PanicOnFail = 1u << 3,
};

inline constexpr KmallocFlags operator|(KmallocFlags left, KmallocFlags right)
{
    return static_cast<KmallocFlags>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

inline constexpr KmallocFlags operator&(KmallocFlags left, KmallocFlags right)
{
    return static_cast<KmallocFlags>(static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
}

inline constexpr KmallocFlags& operator|=(KmallocFlags& left, KmallocFlags right)
{
    left = left | right;
    return left;
}

struct KmemCache;

struct KmemCacheStats
{
    const char* name = nullptr;
    size_t object_size = 0;
    size_t alignment = 0;
    uint32_t slab_count = 0;
    uint32_t empty_slab_count = 0;
    uint32_t partial_slab_count = 0;
    uint32_t full_slab_count = 0;
    uint32_t total_object_count = 0;
    uint32_t free_object_count = 0;
    uint32_t live_object_count = 0;
    uint32_t peak_live_object_count = 0;
    uint64_t alloc_count = 0;
    uint64_t free_count = 0;
    uint64_t failed_alloc_count = 0;
    uint64_t slab_growth_count = 0;
    uint64_t slab_return_count = 0;
};

struct KmemGlobalStats
{
    size_t cache_count = 0;
    uint64_t slab_page_count = 0;
    uint64_t alloc_count = 0;
    uint64_t free_count = 0;
    uint64_t failed_alloc_count = 0;
    uint64_t live_large_allocation_count = 0;
    uint64_t live_large_allocation_bytes = 0;
    uint64_t failed_large_alloc_count = 0;
    uint64_t invalid_free_count = 0;
    uint64_t corruption_count = 0;
};

void kmem_init(PageFrameContainer& frames);
[[nodiscard]] void* kmalloc(size_t size, KmallocFlags flags = KmallocFlags::None);
[[nodiscard]] void* kcalloc(size_t count, size_t size, KmallocFlags flags = KmallocFlags::None);
[[nodiscard]] KmemCache* kmem_cache_create(const char* name, size_t object_size, size_t alignment);
[[nodiscard]] void* kmem_cache_alloc(KmemCache* cache, KmallocFlags flags = KmallocFlags::None);
void kmem_cache_free(KmemCache* cache, void* ptr);
[[nodiscard]] bool kmem_cache_destroy(KmemCache* cache);
void kfree(void* ptr);
[[nodiscard]] bool kmem_allocation_usable_size(const void* ptr, size_t& size);
[[nodiscard]] size_t kmem_cache_stats_count();
[[nodiscard]] bool kmem_get_cache_stats(size_t index, KmemCacheStats& stats);
void kmem_get_global_stats(KmemGlobalStats& stats);
void kmem_dump_stats();