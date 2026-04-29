#include "freestanding/string.hpp"

#include <gtest/gtest.h>

#include <array>

TEST(FreestandingBytes, CopiesAndZeroesExactRanges)
{
    std::array<uint8_t, 5> source{1, 2, 3, 4, 5};
    std::array<uint8_t, 5> destination{};

    freestanding::copy_bytes(destination.data(), source.data(), source.size());
    EXPECT_EQ(source, destination);

    freestanding::zero_bytes(destination.data() + 1, 3);
    EXPECT_EQ((std::array<uint8_t, 5>{1, 0, 0, 0, 5}), destination);
}

TEST(FreestandingString, MeasuresNullEmptyAndNormalStrings)
{
    EXPECT_EQ(0u, freestanding::string_length(nullptr));
    EXPECT_EQ(0u, freestanding::string_length(""));
    EXPECT_EQ(5u, freestanding::string_length("hello"));
}

TEST(FreestandingString, CopyStringTerminatesAndTruncates)
{
    char buffer[5] = {'x', 'x', 'x', 'x', 'x'};
    freestanding::copy_string(buffer, sizeof(buffer), "abcdef");
    EXPECT_STREQ("abcd", buffer);
    EXPECT_EQ('\0', buffer[4]);

    freestanding::copy_string(buffer, sizeof(buffer), nullptr);
    EXPECT_STREQ("", buffer);
}

TEST(FreestandingString, AppendStringTerminatesAndTruncates)
{
    char buffer[8]{};
    freestanding::copy_string(buffer, sizeof(buffer), "abc");
    freestanding::append_string(buffer, sizeof(buffer), "defgh");
    EXPECT_STREQ("abcdefg", buffer);
    EXPECT_EQ('\0', buffer[7]);
}

TEST(FreestandingString, ComparesStringsAndSuffixes)
{
    EXPECT_TRUE(freestanding::strings_equal("", ""));
    EXPECT_TRUE(freestanding::strings_equal("abc", "abc"));
    EXPECT_FALSE(freestanding::strings_equal("abc", "abd"));
    EXPECT_FALSE(freestanding::strings_equal(nullptr, "abc"));
    EXPECT_FALSE(freestanding::strings_equal("abc", nullptr));

    EXPECT_TRUE(freestanding::path_ends_with("/boot/kernel.elf", "kernel.elf"));
    EXPECT_TRUE(freestanding::path_ends_with("kernel.elf", "kernel.elf"));
    EXPECT_FALSE(freestanding::path_ends_with("abc", "abcd"));
    EXPECT_FALSE(freestanding::path_ends_with(nullptr, "abc"));
}
