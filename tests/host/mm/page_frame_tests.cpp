#include "mm/page_frame.hpp"

#include "handoff/memory_layout.h"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>

namespace
{
constexpr uint64_t kArenaBytes = 16ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x300000;

std::array<BootMemoryRegion, 1> usable_memory()
{
    return {{
        {
            .physical_start = 0,
            .length = kArenaBytes,
            .type = BootMemoryType::Usable,
            .attributes = 0,
        },
    }};
}

PageFrameContainer initialized_frames(os1::host_test::PhysicalMemoryArena& arena)
{
    (void)arena;
    PageFrameContainer frames;
    auto regions = usable_memory();
    EXPECT_TRUE(frames.initialize(regions, kBitmapPhysical, kPageFrameBitmapQwordLimit));
    return frames;
}
}  // namespace

TEST(PageFrameContainer, RejectsInvalidInitialization)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames;
    std::array<BootMemoryRegion, 1> reserved{{
        {0, kArenaBytes, BootMemoryType::Reserved, 0},
    }};
    EXPECT_FALSE(frames.initialize(reserved, kBitmapPhysical, kPageFrameBitmapQwordLimit));

    PageFrameContainer unaligned;
    std::array<BootMemoryRegion, 1> bad{{
        {1, kPageSize, BootMemoryType::Usable, 0},
    }};
    EXPECT_FALSE(unaligned.initialize(bad, kBitmapPhysical, kPageFrameBitmapQwordLimit));
}

TEST(PageFrameContainer, InitializesAndReservesBootRanges)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames(arena);

    const uint64_t reserved_pages = (kPageFrameBitmapSizeBytes / kPageSize) +
                                    (kEarlyReservedPhysicalEnd / kPageSize) +
                                    ((kKernelReservedPhysicalEnd - kKernelReservedPhysicalStart) / kPageSize);
    const uint64_t expected_free_pages = (kArenaBytes / kPageSize) - reserved_pages;

    EXPECT_EQ(kArenaBytes, frames.memory_size());
    EXPECT_EQ(kArenaBytes, frames.memory_end());
    EXPECT_EQ(kArenaBytes / kPageSize, frames.page_count());
    EXPECT_EQ(expected_free_pages, frames.free_page_count());

    uint64_t page = 0;
    ASSERT_TRUE(frames.allocate(page));
    EXPECT_EQ(kEarlyReservedPhysicalEnd, page);
    EXPECT_EQ(expected_free_pages - 1u, frames.free_page_count());
}

TEST(PageFrameContainer, AllocatesFreesAndReservesRanges)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames(arena);

    uint64_t first = 0;
    ASSERT_TRUE(frames.allocate(first));
    EXPECT_TRUE(frames.free(first));
    EXPECT_FALSE(frames.free(first + 1));

    uint64_t contiguous = 0;
    ASSERT_TRUE(frames.allocate(contiguous, 3));
    EXPECT_EQ(first, contiguous);

    const uint64_t before_reserve = frames.free_page_count();
    EXPECT_TRUE(frames.reserve_range(0x400000 + 1, kPageSize));
    EXPECT_EQ(before_reserve - 2, frames.free_page_count());
    EXPECT_FALSE(frames.reserve_range(kArenaBytes + kPageSize, kPageSize));
}
