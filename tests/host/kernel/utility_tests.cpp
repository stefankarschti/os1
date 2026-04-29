#include "util/align.hpp"
#include "util/fixed_string.hpp"

#include <gtest/gtest.h>

#include <array>

TEST(AlignHelpers, AlignDownAndUpToPowerOfTwo)
{
    EXPECT_EQ(0x1000ull, align_down(0x1234ull, 0x1000ull));
    EXPECT_EQ(0x2000ull, align_up(0x1234ull, 0x1000ull));
    EXPECT_EQ(0x2000ull, align_down(0x2000ull, 0x1000ull));
    EXPECT_EQ(0x2000ull, align_up(0x2000ull, 0x1000ull));
}

TEST(FixedString, CopiesTruncatesAndPads)
{
    std::array<char, 6> buffer{};
    buffer.fill('x');

    copy_fixed_string(buffer.data(), buffer.size(), "abcdef");
    EXPECT_EQ('a', buffer[0]);
    EXPECT_EQ('b', buffer[1]);
    EXPECT_EQ('c', buffer[2]);
    EXPECT_EQ('d', buffer[3]);
    EXPECT_EQ('e', buffer[4]);
    EXPECT_EQ('\0', buffer[5]);

    buffer.fill('x');
    copy_fixed_string(buffer.data(), buffer.size(), "hi");
    EXPECT_EQ('h', buffer[0]);
    EXPECT_EQ('i', buffer[1]);
    for(size_t i = 2; i < buffer.size(); ++i)
    {
        EXPECT_EQ('\0', buffer[i]);
    }
}
