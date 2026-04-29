#include <os1/observe.h>

#include <gtest/gtest.h>

#include <stddef.h>

TEST(ObserveAbi, ConstantsAreStable)
{
    EXPECT_EQ(1, OS1_OBSERVE_ABI_VERSION);
    EXPECT_EQ(1, OS1_OBSERVE_SYSTEM);
    EXPECT_EQ(2, OS1_OBSERVE_PROCESSES);
    EXPECT_EQ(3, OS1_OBSERVE_CPUS);
    EXPECT_EQ(4, OS1_OBSERVE_PCI);
    EXPECT_EQ(5, OS1_OBSERVE_INITRD);
}

TEST(ObserveAbi, HeaderLayoutIsPacked)
{
    EXPECT_EQ(16u, sizeof(Os1ObserveHeader));
    EXPECT_EQ(0u, offsetof(Os1ObserveHeader, abi_version));
    EXPECT_EQ(4u, offsetof(Os1ObserveHeader, kind));
    EXPECT_EQ(8u, offsetof(Os1ObserveHeader, record_size));
    EXPECT_EQ(12u, offsetof(Os1ObserveHeader, record_count));
}

TEST(ObserveAbi, RecordLayoutsArePacked)
{
    EXPECT_EQ(128u, sizeof(Os1ObserveSystemRecord));
    EXPECT_EQ(64u, offsetof(Os1ObserveSystemRecord, bootloader_name));

    EXPECT_EQ(68u, sizeof(Os1ObserveProcessRecord));
    EXPECT_EQ(36u, offsetof(Os1ObserveProcessRecord, name));

    EXPECT_EQ(28u, sizeof(Os1ObserveCpuRecord));
    EXPECT_EQ(12u, offsetof(Os1ObserveCpuRecord, current_pid));

    EXPECT_EQ(17u, sizeof(Os1ObservePciBar));
    EXPECT_EQ(119u, sizeof(Os1ObservePciRecord));
    EXPECT_EQ(17u, offsetof(Os1ObservePciRecord, bars));

    EXPECT_EQ(72u, sizeof(Os1ObserveInitrdRecord));
    EXPECT_EQ(64u, offsetof(Os1ObserveInitrdRecord, size));
}
