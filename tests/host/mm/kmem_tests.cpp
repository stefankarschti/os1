#include "mm/kmem.hpp"

#include "handoff/memory_layout.h"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
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
}  // namespace

TEST(Kmem, RejectsZeroAndOversizedRequests)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    kfree(nullptr);
    EXPECT_EQ(nullptr, kmalloc(0));
    EXPECT_EQ(nullptr, kmalloc(1025));
}

TEST(Kmem, AllocatesAlignedObjectsAndReusesFreedSlots)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    void* first = kmalloc(17);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(first) % 32u);

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