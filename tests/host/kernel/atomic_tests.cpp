#include "sync/atomic.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>
#include <vector>

TEST(AtomicWrappers, StoreReleaseAndLoadAcquireRoundTrip)
{
    volatile uint64_t value = 0;

    atomic_store_release(&value, static_cast<uint64_t>(0x123456789abcdef0ull));

    EXPECT_EQ(0x123456789abcdef0ull, atomic_load_acquire(&value));
}

TEST(AtomicWrappers, CompareExchangeStrongUpdatesExpectedOnFailure)
{
    volatile uint32_t value = 7;
    uint32_t expected = 7;

    EXPECT_TRUE(atomic_compare_exchange_strong(&value, &expected, 11u));
    EXPECT_EQ(11u, atomic_load_acquire(&value));
    EXPECT_EQ(7u, expected);

    expected = 9;
    EXPECT_FALSE(atomic_compare_exchange_strong(&value, &expected, 13u));
    EXPECT_EQ(11u, expected);
    EXPECT_EQ(11u, atomic_load_acquire(&value));
}

TEST(AtomicWrappers, FetchAddIsLinearUnderConcurrentThreads)
{
    constexpr uint64_t kThreadCount = 8;
    constexpr uint64_t kIterations = 4096;
    volatile uint64_t counter = 0;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for(uint64_t thread = 0; thread < kThreadCount; ++thread)
    {
        threads.emplace_back([&counter]() {
            for(uint64_t iteration = 0; iteration < kIterations; ++iteration)
            {
                (void)atomic_fetch_add(&counter, uint64_t{1});
            }
        });
    }

    for(std::thread& thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(kThreadCount * kIterations, atomic_load_acquire(&counter));
}

TEST(AtomicWrappers, ExchangeReturnsPreviousValue)
{
    volatile uint16_t value = 3;

    EXPECT_EQ(3u, atomic_exchange(&value, static_cast<uint16_t>(8)));
    EXPECT_EQ(8u, atomic_load_acquire(&value));
}
