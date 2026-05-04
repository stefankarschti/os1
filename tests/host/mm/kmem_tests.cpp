#include "mm/kmem.hpp"

#include "handoff/memory_layout.h"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
constexpr uint64_t kArenaBytes = 16ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x300000;

PageFrameContainer initialized_frames()
{
    std::array<BootMemoryRegion, 1> regions{{
        {
            .physical_start = 0,
            .length = kArenaBytes,
            .type = BootMemoryType::Usable,
            .attributes = 0,
        },
    }};

    PageFrameContainer frames;
    EXPECT_TRUE(frames.initialize(regions, kBitmapPhysical, kPageFrameBitmapQwordLimit));
    return frames;
}

bool find_cache_stats(size_t object_size, KmemCacheStats& stats)
{
    for(size_t index = 0; index < kmem_cache_stats_count(); ++index)
    {
        if(kmem_get_cache_stats(index, stats) && (stats.object_size == object_size))
        {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST(Kmem, RejectsZeroAndOverflowedRequests)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    kfree(nullptr);
    EXPECT_EQ(nullptr, kmalloc(0));
    EXPECT_EQ(nullptr, kmalloc(std::numeric_limits<size_t>::max()));
    EXPECT_EQ(nullptr, kcalloc(std::numeric_limits<size_t>::max(), 2));
}

TEST(Kmem, AllocatesAlignedObjectsAndReusesFreedSlots)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    void* first = kmalloc(17);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(first) % 32u);

    size_t usable_size = 0;
    ASSERT_TRUE(kmem_allocation_usable_size(first, usable_size));
    EXPECT_EQ(32u, usable_size);

    kfree(first);

    void* second = kmalloc(17, KmallocFlags::NoGrow);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(first, second);

    kfree(second);
}

TEST(Kmem, GrowsCachesAndKeepsOneEmptySlab)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    const uint64_t initial_free_pages = frames.free_page_count();
    std::vector<void*> allocations;

    while(frames.free_page_count() > (initial_free_pages - 2u))
    {
        void* ptr = kmalloc(16);
        ASSERT_NE(nullptr, ptr);
        allocations.push_back(ptr);
    }

    EXPECT_EQ(initial_free_pages - 2u, frames.free_page_count());

    for(void* ptr : allocations)
    {
        kfree(ptr);
    }

    EXPECT_EQ(initial_free_pages - 1u, frames.free_page_count());
}

TEST(Kmem, KcallocZeroesMemory)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    auto* words = static_cast<uint64_t*>(kcalloc(8, sizeof(uint64_t)));
    ASSERT_NE(nullptr, words);
    for(size_t index = 0; index < 8; ++index)
    {
        EXPECT_EQ(0u, words[index]);
    }

    size_t usable_size = 0;
    ASSERT_TRUE(kmem_allocation_usable_size(words, usable_size));
    EXPECT_EQ(64u, usable_size);

    kfree(words);
}

TEST(Kmem, LargeAllocationsUsePageRuns)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    const uint64_t initial_free_pages = frames.free_page_count();
    void* allocation = kmalloc(5000);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(initial_free_pages - 2u, frames.free_page_count());

    size_t usable_size = 0;
    ASSERT_TRUE(kmem_allocation_usable_size(allocation, usable_size));
    EXPECT_GE(usable_size, 5000u);
    EXPECT_LT(usable_size, 2u * static_cast<size_t>(kPageSize));

    KmemGlobalStats global{};
    kmem_get_global_stats(global);
    EXPECT_EQ(1u, global.live_large_allocation_count);
    EXPECT_EQ(5000u, global.live_large_allocation_bytes);
    EXPECT_EQ(1u, global.alloc_count);
    EXPECT_EQ(0u, global.failed_large_alloc_count);

    kfree(allocation);
    EXPECT_EQ(initial_free_pages, frames.free_page_count());

    kmem_get_global_stats(global);
    EXPECT_EQ(0u, global.live_large_allocation_count);
    EXPECT_EQ(0u, global.live_large_allocation_bytes);
    EXPECT_EQ(1u, global.free_count);
}

TEST(Kmem, StatsTrackCacheFailuresAndReturns)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    std::vector<void*> allocations;
    void* first = kmalloc(17);
    ASSERT_NE(nullptr, first);
    allocations.push_back(first);

    for(;;)
    {
        void* ptr = kmalloc(17, KmallocFlags::NoGrow);
        if(nullptr == ptr)
        {
            break;
        }
        allocations.push_back(ptr);
    }
    EXPECT_FALSE(allocations.empty());
    EXPECT_EQ(nullptr, kmalloc(5000, KmallocFlags::NoGrow));

    KmemCacheStats cache{};
    ASSERT_TRUE(find_cache_stats(32, cache));
    EXPECT_EQ(allocations.size(), cache.alloc_count);
    EXPECT_EQ(allocations.size(), cache.live_object_count);
    EXPECT_EQ(1u, cache.failed_alloc_count);

    KmemGlobalStats global{};
    kmem_get_global_stats(global);
    EXPECT_EQ(2u, global.failed_alloc_count);
    EXPECT_EQ(1u, global.failed_large_alloc_count);

    for(void* ptr : allocations)
    {
        kfree(ptr);
    }

    ASSERT_TRUE(find_cache_stats(32, cache));
    EXPECT_EQ(0u, cache.live_object_count);
    EXPECT_EQ(cache.alloc_count, cache.free_count);
    EXPECT_GE(cache.slab_return_count, 0u);

    kmem_dump_stats();
}

TEST(Kmem, NamedCacheAllocatesAlignedObjectsAndDestroys)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    const size_t initial_cache_count = kmem_cache_stats_count();
    const uint64_t initial_free_pages = frames.free_page_count();

    KmemCache* cache = kmem_cache_create("test-object-80", 80, 64);
    ASSERT_NE(nullptr, cache);
    EXPECT_EQ(initial_cache_count + 1u, kmem_cache_stats_count());

    void* object = kmem_cache_alloc(cache);
    ASSERT_NE(nullptr, object);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(object) % 64u);
    EXPECT_EQ(initial_free_pages - 1u, frames.free_page_count());

    size_t usable_size = 0;
    ASSERT_TRUE(kmem_allocation_usable_size(object, usable_size));
    EXPECT_EQ(128u, usable_size);

    KmemCacheStats stats{};
    ASSERT_TRUE(find_cache_stats(80, stats));
    EXPECT_STREQ("test-object-80", stats.name);
    EXPECT_EQ(64u, stats.alignment);
    EXPECT_EQ(1u, stats.live_object_count);
    EXPECT_EQ(1u, stats.alloc_count);

    kmem_cache_free(cache, object);
    ASSERT_TRUE(find_cache_stats(80, stats));
    EXPECT_EQ(0u, stats.live_object_count);
    EXPECT_EQ(1u, stats.free_count);

    EXPECT_TRUE(kmem_cache_destroy(cache));
    EXPECT_EQ(initial_cache_count, kmem_cache_stats_count());
    EXPECT_EQ(initial_free_pages, frames.free_page_count());
}

TEST(Kmem, NamedCacheDestroyFailsWhileObjectsRemainLive)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    KmemCache* cache = kmem_cache_create("test-object-96", 96, 32);
    ASSERT_NE(nullptr, cache);

    void* object = kmem_cache_alloc(cache);
    ASSERT_NE(nullptr, object);

    EXPECT_FALSE(kmem_cache_destroy(cache));
    kfree(object);
    EXPECT_TRUE(kmem_cache_destroy(cache));
}

TEST(Kmem, NamedCacheRejectsInvalidParameters)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    EXPECT_EQ(nullptr, kmem_cache_create(nullptr, 64, 16));
    EXPECT_EQ(nullptr, kmem_cache_create("", 64, 16));
    EXPECT_EQ(nullptr, kmem_cache_create("too-small", sizeof(void*) - 1u, 16));
    EXPECT_EQ(nullptr, kmem_cache_create("bad-align", 64, 24));
    EXPECT_EQ(nullptr, kmem_cache_create("too-large-align", 64, 128));
    EXPECT_EQ(nullptr, kmem_cache_create("too-large-slot", kPageSize, 16));
}

TEST(Kmem, NoGrowStopsAtCurrentSlabCapacity)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    const uint64_t initial_free_pages = frames.free_page_count();
    std::vector<void*> allocations;

    void* first = kmalloc(17);
    ASSERT_NE(nullptr, first);
    allocations.push_back(first);
    EXPECT_EQ(initial_free_pages - 1u, frames.free_page_count());

    for(;;)
    {
        void* ptr = kmalloc(17, KmallocFlags::NoGrow);
        if(nullptr == ptr)
        {
            break;
        }
        allocations.push_back(ptr);
    }

    EXPECT_EQ(initial_free_pages - 1u, frames.free_page_count());

    for(void* ptr : allocations)
    {
        kfree(ptr);
    }

    EXPECT_EQ(initial_free_pages - 1u, frames.free_page_count());
}