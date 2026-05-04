#include "mm/dma.hpp"
#include "mm/kmem.hpp"

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
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);
    platform_reset_state();

    DmaBuffer buffer{};
    ASSERT_TRUE(dma_allocate_buffer(
        frames, DeviceId{DeviceBus::Pci, 5}, 6000, DmaDirection::Bidirectional, buffer));
    ASSERT_TRUE(buffer.active);
    EXPECT_EQ(2u, buffer.page_count);
    EXPECT_NE(nullptr, buffer.virtual_address);
    EXPECT_EQ(1u, dma_allocation_count());

    const DmaAllocationRecord* records = dma_allocation_records();
    ASSERT_NE(nullptr, records);
    EXPECT_TRUE(records[0].active);

    dma_release_buffer(frames, buffer);
    EXPECT_FALSE(buffer.active);
    EXPECT_EQ(0u, dma_allocation_count());

    platform_reset_state();
}

TEST(DmaBuffer, AllocationsGrowPastLegacyLimit)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);
    platform_reset_state();

    constexpr size_t kAllocationTarget = kPlatformMaxDmaAllocations + 4u;
    std::array<DmaBuffer, kAllocationTarget> buffers{};
    for(size_t i = 0; i < buffers.size(); ++i)
    {
        ASSERT_TRUE(dma_allocate_buffer(frames,
                                        DeviceId{DeviceBus::Pci, static_cast<uint16_t>(i)},
                                        kPageSize,
                                        DmaDirection::Bidirectional,
                                        buffers[i]));
    }

    EXPECT_EQ(kAllocationTarget, dma_allocation_count());

    for(auto& buffer : buffers)
    {
        dma_release_buffer(frames, buffer);
    }
    EXPECT_EQ(0u, dma_allocation_count());

    platform_reset_state();
}
