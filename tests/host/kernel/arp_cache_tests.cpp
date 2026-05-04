#include "drivers/net/arp_cache.hpp"

#include "mm/kmem.hpp"

#include "handoff/memory_layout.h"
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

bool find_cache_stats(const char* name, KmemCacheStats& stats)
{
    for(size_t index = 0; index < kmem_cache_stats_count(); ++index)
    {
        if(kmem_get_cache_stats(index, stats) && (nullptr != stats.name) && (0 == strcmp(stats.name, name)))
        {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST(ArpCache, LookupMissesWhenEmpty)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ArpCache cache;
    ASSERT_TRUE(cache.initialize("arp_entry"));

    const uint8_t ip[]{10u, 0u, 2u, 2u};
    uint8_t mac[6]{};
    EXPECT_FALSE(cache.lookup(ip, mac));

    EXPECT_TRUE(cache.destroy());
}

TEST(ArpCache, StoresEntriesInNamedCache)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    const size_t initial_cache_count = kmem_cache_stats_count();

    ArpCache cache;
    ASSERT_TRUE(cache.initialize("arp_entry"));
    EXPECT_EQ(initial_cache_count + 1u, kmem_cache_stats_count());

    const uint8_t ip[]{10u, 0u, 2u, 2u};
    const uint8_t mac[]{0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
    ASSERT_TRUE(cache.upsert(ip, mac));

    uint8_t resolved_mac[6]{};
    ASSERT_TRUE(cache.lookup(ip, resolved_mac));
    EXPECT_EQ(0, std::memcmp(resolved_mac, mac, sizeof(mac)));
    EXPECT_EQ(1u, cache.entry_count());

    KmemCacheStats stats{};
    ASSERT_TRUE(find_cache_stats("arp_entry", stats));
    EXPECT_EQ(1u, stats.live_object_count);
    EXPECT_EQ(1u, stats.alloc_count);

    ASSERT_TRUE(cache.destroy());
    EXPECT_EQ(initial_cache_count, kmem_cache_stats_count());
}

TEST(ArpCache, UpdatesExistingEntriesWithoutGrowing)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    PageFrameContainer frames = initialized_frames();
    kmem_init(frames);

    ArpCache cache;
    ASSERT_TRUE(cache.initialize("arp_entry"));

    const uint8_t ip[]{10u, 0u, 2u, 2u};
    const uint8_t first_mac[]{0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
    const uint8_t second_mac[]{0x52u, 0x54u, 0x00u, 0xAAu, 0xBBu, 0xCCu};
    ASSERT_TRUE(cache.upsert(ip, first_mac));
    ASSERT_TRUE(cache.upsert(ip, second_mac));

    uint8_t resolved_mac[6]{};
    ASSERT_TRUE(cache.lookup(ip, resolved_mac));
    EXPECT_EQ(0, std::memcmp(resolved_mac, second_mac, sizeof(second_mac)));
    EXPECT_EQ(1u, cache.entry_count());

    KmemCacheStats stats{};
    ASSERT_TRUE(find_cache_stats("arp_entry", stats));
    EXPECT_EQ(1u, stats.live_object_count);
    EXPECT_EQ(1u, stats.alloc_count);

    EXPECT_TRUE(cache.destroy());
}