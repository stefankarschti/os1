#include "fs/cpio_newc.hpp"

#include <gtest/gtest.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

namespace
{
constexpr uint32_t kRegularFileMode = 0100000u | 0644u;
constexpr uint32_t kDirectoryMode = 0040000u | 0755u;

void append_hex8(std::vector<uint8_t>& archive, uint32_t value)
{
    char text[9]{};
    snprintf(text, sizeof(text), "%08X", value);
    archive.insert(archive.end(), text, text + 8);
}

void pad4(std::vector<uint8_t>& archive)
{
    while((archive.size() % 4) != 0)
    {
        archive.push_back(0);
    }
}

void append_entry(std::vector<uint8_t>& archive,
                  const std::string& name,
                  uint32_t mode,
                  const std::vector<uint8_t>& data)
{
    const std::string stored_name = name + '\0';
    archive.insert(archive.end(), {'0', '7', '0', '7', '0', '1'});
    append_hex8(archive, 1);
    append_hex8(archive, mode);
    append_hex8(archive, 0);
    append_hex8(archive, 0);
    append_hex8(archive, 1);
    append_hex8(archive, 0);
    append_hex8(archive, static_cast<uint32_t>(data.size()));
    append_hex8(archive, 0);
    append_hex8(archive, 0);
    append_hex8(archive, 0);
    append_hex8(archive, 0);
    append_hex8(archive, static_cast<uint32_t>(stored_name.size()));
    append_hex8(archive, 0);
    archive.insert(archive.end(), stored_name.begin(), stored_name.end());
    pad4(archive);
    archive.insert(archive.end(), data.begin(), data.end());
    pad4(archive);
}

std::vector<uint8_t> make_archive()
{
    std::vector<uint8_t> archive;
    append_entry(archive, ".", kDirectoryMode, {});
    append_entry(archive, "./bin/init", kRegularFileMode, {'i', 'n', 'i', 't'});
    append_entry(archive, "/bin/sh", kRegularFileMode, {'s', 'h'});
    append_entry(archive, "TRAILER!!!", 0, {});
    return archive;
}

struct VisitContext
{
    std::vector<std::string> names;
    std::vector<std::vector<uint8_t>> files;
};

bool collect_file(const char* archive_name, const uint8_t* file_data, uint64_t file_size, void* context)
{
    auto* visit = static_cast<VisitContext*>(context);
    visit->names.emplace_back(archive_name);
    visit->files.emplace_back(file_data, file_data + file_size);
    return true;
}
}  // namespace

TEST(CpioNewcParser, VisitsRegularFilesOnly)
{
    auto archive = make_archive();
    VisitContext context{};

    ASSERT_TRUE(for_each_cpio_newc_file(archive.data(), archive.size(), collect_file, &context));
    ASSERT_EQ(2u, context.names.size());
    EXPECT_EQ("./bin/init", context.names[0]);
    EXPECT_EQ("/bin/sh", context.names[1]);
    EXPECT_EQ((std::vector<uint8_t>{'i', 'n', 'i', 't'}), context.files[0]);
    EXPECT_EQ((std::vector<uint8_t>{'s', 'h'}), context.files[1]);
}

TEST(CpioNewcParser, NormalizesPathComparisonAndCopy)
{
    EXPECT_TRUE(cpio_newc_paths_equal("./bin/init", "/bin/init"));
    EXPECT_TRUE(cpio_newc_paths_equal("/bin/init", "bin/init"));
    EXPECT_FALSE(cpio_newc_paths_equal("/bin/init", "/bin/sh"));

    char path[8]{};
    copy_cpio_newc_path(path, sizeof(path), "./bin/init");
    EXPECT_STREQ("/bin/in", path);

    char empty[4] = {'x', 'x', 'x', 'x'};
    copy_cpio_newc_path(empty, sizeof(empty), nullptr);
    EXPECT_STREQ("", empty);
}

TEST(CpioNewcParser, RejectsMalformedArchives)
{
    auto archive = make_archive();
    VisitContext context{};

    EXPECT_FALSE(for_each_cpio_newc_file(nullptr, archive.size(), collect_file, &context));
    EXPECT_FALSE(for_each_cpio_newc_file(archive.data(), archive.size(), nullptr, &context));
    EXPECT_FALSE(for_each_cpio_newc_file(archive.data(), 10, collect_file, &context));

    auto bad_magic = archive;
    bad_magic[0] = '1';
    EXPECT_FALSE(for_each_cpio_newc_file(bad_magic.data(), bad_magic.size(), collect_file, &context));

    auto bad_hex = archive;
    bad_hex[6 + 8] = 'Z';
    EXPECT_FALSE(for_each_cpio_newc_file(bad_hex.data(), bad_hex.size(), collect_file, &context));

    auto missing_nul = archive;
    const size_t first_name_nul_offset = 111;
    missing_nul[first_name_nul_offset] = 'x';
    EXPECT_FALSE(
        for_each_cpio_newc_file(missing_nul.data(), missing_nul.size(), collect_file, &context));
}

TEST(CpioNewcParser, VisitorCanStopTraversal)
{
    auto archive = make_archive();
    auto stop = [](const char*, const uint8_t*, uint64_t, void*) -> bool { return false; };
    EXPECT_FALSE(for_each_cpio_newc_file(archive.data(), archive.size(), stop, nullptr));
}
