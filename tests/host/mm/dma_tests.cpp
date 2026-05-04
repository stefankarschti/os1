#include "mm/dma.hpp"

#include "handoff/memory_layout.h"
#include "platform/state.hpp"
#include "support/physical_memory.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstring>

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

TEST(DmaBuffer, AllocateAndReleasePublishesOwnership)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    std::memset(&g_platform, 0, sizeof(g_platform));
    PageFrameContainer frames = initialized_frames();

    DmaBuffer buffer{};
    ASSERT_TRUE(dma_allocate_buffer(
        frames, DeviceId{DeviceBus::Pci, 5}, 6000, DmaDirection::Bidirectional, buffer));
    ASSERT_TRUE(buffer.active);
    EXPECT_EQ(2u, buffer.page_count);
    EXPECT_NE(nullptr, buffer.virtual_address);
    EXPECT_EQ(1u, g_platform.dma_allocation_count);
    EXPECT_TRUE(g_platform.dma_allocations[0].active);

    dma_release_buffer(frames, buffer);
    EXPECT_FALSE(buffer.active);
    EXPECT_FALSE(g_platform.dma_allocations[0].active);
}
