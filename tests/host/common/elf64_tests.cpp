#include "elf/elf64.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace
{
std::vector<uint8_t> make_image(uint16_t program_count = 1)
{
    std::vector<uint8_t> image(sizeof(elf::Elf64Header) +
                               program_count * sizeof(elf::Elf64ProgramHeader) + 64);
    auto* header = reinterpret_cast<elf::Elf64Header*>(image.data());
    header->magic = elf::kMagic;
    header->type = elf::kTypeExec;
    header->machine = elf::kMachineX86_64;
    header->phoff = sizeof(elf::Elf64Header);
    header->ehsize = sizeof(elf::Elf64Header);
    header->phentsize = sizeof(elf::Elf64ProgramHeader);
    header->phnum = program_count;
    return image;
}
}  // namespace

TEST(Elf64HeaderFromImage, RejectsNullAndTooSmallImages)
{
    EXPECT_EQ(nullptr, elf::header_from_image(nullptr, sizeof(elf::Elf64Header)));
    std::vector<uint8_t> image(sizeof(elf::Elf64Header) - 1);
    EXPECT_EQ(nullptr, elf::header_from_image(image.data(), image.size()));
}

TEST(Elf64HeaderFromImage, RejectsBadMagicProgramOffsetAndEntrySize)
{
    auto image = make_image();
    auto* header = reinterpret_cast<elf::Elf64Header*>(image.data());

    header->magic = 0;
    EXPECT_EQ(nullptr, elf::header_from_image(image.data(), image.size()));

    header->magic = elf::kMagic;
    header->phoff = image.size();
    EXPECT_EQ(nullptr, elf::header_from_image(image.data(), image.size()));

    header->phoff = sizeof(elf::Elf64Header);
    header->phentsize = sizeof(elf::Elf64ProgramHeader) - 1;
    EXPECT_EQ(nullptr, elf::header_from_image(image.data(), image.size()));
}

TEST(Elf64HeaderFromImage, AcceptsMinimalValidHeader)
{
    auto image = make_image();
    const auto* header = elf::header_from_image(image.data(), image.size());
    ASSERT_NE(nullptr, header);
    EXPECT_EQ(elf::kTypeExec, header->type);
    EXPECT_EQ(elf::kMachineX86_64, header->machine);
}

TEST(Elf64ProgramHeaderFromImage, ReturnsExpectedProgramHeaderPointers)
{
    auto image = make_image(2);
    const auto* header = elf::header_from_image(image.data(), image.size());
    ASSERT_NE(nullptr, header);

    const auto* first = elf::program_header_from_image(*header, image.data(), image.size(), 0);
    const auto* second = elf::program_header_from_image(*header, image.data(), image.size(), 1);

    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(first), image.data() + sizeof(elf::Elf64Header));
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(second),
              image.data() + sizeof(elf::Elf64Header) + sizeof(elf::Elf64ProgramHeader));
}

TEST(Elf64ProgramHeaderFromImage, RejectsOutOfBoundsAndOverflow)
{
    auto image = make_image(1);
    auto header = *reinterpret_cast<elf::Elf64Header*>(image.data());

    EXPECT_EQ(nullptr, elf::program_header_from_image(header, image.data(), image.size(), 5));

    header.phoff = std::numeric_limits<uint64_t>::max() - 8;
    EXPECT_EQ(nullptr, elf::program_header_from_image(header, image.data(), image.size(), 1));
}

TEST(Elf64SegmentBounds, ValidatesLoadableFileBounds)
{
    elf::Elf64ProgramHeader program{};
    program.offset = 16;
    program.filesz = 8;
    program.memsz = 16;
    EXPECT_TRUE(elf::loadable_segment_bounds_valid(program, 32));

    program.memsz = 7;
    EXPECT_FALSE(elf::loadable_segment_bounds_valid(program, 32));

    program.memsz = 16;
    program.offset = 28;
    EXPECT_FALSE(elf::loadable_segment_bounds_valid(program, 32));

    program.offset = std::numeric_limits<uint64_t>::max() - 1;
    program.filesz = 8;
    EXPECT_FALSE(elf::loadable_segment_bounds_valid(program, std::numeric_limits<uint64_t>::max()));
}
