#include "handoff/boot_info.hpp"
#include "handoff/memory_layout.h"
#include "mm/virtual_memory.hpp"
#include "platform/acpica_integration.hpp"
#include "support/physical_memory.hpp"

extern "C"
{
#include "acpi.h"
}

#include <gtest/gtest.h>

#include <array>
#include <cstring>

namespace
{
constexpr uint64_t kArenaBytes = 2ull * 1024ull * 1024ull;
constexpr uint64_t kBitmapPhysical = 0x100000;
constexpr uint64_t kRsdpPhysical = 0x1000;
constexpr uint64_t kXsdtPhysical = 0x2000;
constexpr uint64_t kMadtPhysical = 0x3000;
constexpr uint64_t kFadtPhysical = 0x4000;
constexpr uint64_t kDsdtPhysical = 0x5000;

struct [[gnu::packed]] TestRsdp
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct [[gnu::packed]] TestSdtHeader
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct [[gnu::packed]] TestXsdt
{
    TestSdtHeader header;
    uint64_t entries[2];
};

struct [[gnu::packed]] TestMadt
{
    TestSdtHeader header;
    uint32_t lapic_address;
    uint32_t flags;
};

struct [[gnu::packed]] TestGas
{
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
};

struct [[gnu::packed]] TestFadt
{
    TestSdtHeader header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved0;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_request;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_block_length;
    uint8_t gpe1_block_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t c2_latency;
    uint16_t c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_architecture_flags;
    uint8_t reserved1;
    uint32_t flags;
    TestGas reset_register;
    uint8_t reset_value;
    uint16_t arm_boot_architecture_flags;
    uint8_t minor_version;
    uint64_t x_firmware_control;
    uint64_t x_dsdt;
};

uint8_t checksum_value(const void* data, size_t length)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for(size_t index = 0; index < length; ++index)
    {
        sum = static_cast<uint8_t>(sum + bytes[index]);
    }
    return static_cast<uint8_t>(0u - sum);
}

void initialize_header(TestSdtHeader& header, const char* signature, uint32_t length)
{
    std::memset(&header, 0, sizeof(header));
    std::memcpy(header.signature, signature, 4);
    std::memcpy(header.oem_id, "OS1   ", 6);
    std::memcpy(header.oem_table_id, "OS1ACPIC", 8);
    header.length = length;
    header.revision = 1;
    header.oem_revision = 1;
    header.creator_id = 0x314F5331u;
    header.creator_revision = 1;
}

void finalize_sdt(TestSdtHeader& header)
{
    header.checksum = 0;
    header.checksum = checksum_value(&header, header.length);
}

PageFrameContainer make_frames()
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

BootInfo make_boot_info(uint64_t rsdp_physical)
{
    BootInfo info{};
    info.magic = kBootInfoMagic;
    info.version = kBootInfoVersion;
    info.source = BootSource::TestHarness;
    info.rsdp_physical = rsdp_physical;
    return info;
}

void build_tables(os1::host_test::PhysicalMemoryArena& arena,
                  bool include_fadt,
                  bool corrupt_rsdp_checksum)
{
    auto* rsdp = reinterpret_cast<TestRsdp*>(arena.data() + kRsdpPhysical);
    auto* xsdt = reinterpret_cast<TestXsdt*>(arena.data() + kXsdtPhysical);
    auto* madt = reinterpret_cast<TestMadt*>(arena.data() + kMadtPhysical);
    auto* fadt = reinterpret_cast<TestFadt*>(arena.data() + kFadtPhysical);
    auto* dsdt = reinterpret_cast<TestSdtHeader*>(arena.data() + kDsdtPhysical);

    std::memset(rsdp, 0, sizeof(*rsdp));
    std::memcpy(rsdp->signature, "RSD PTR ", 8);
    std::memcpy(rsdp->oem_id, "OS1   ", 6);
    rsdp->revision = 2;
    rsdp->length = sizeof(*rsdp);
    rsdp->xsdt_address = kXsdtPhysical;
    rsdp->checksum = checksum_value(rsdp, 20);
    rsdp->extended_checksum = checksum_value(rsdp, sizeof(*rsdp));
    if(corrupt_rsdp_checksum)
    {
        ++rsdp->checksum;
        ++rsdp->extended_checksum;
    }

    std::memset(xsdt, 0, sizeof(*xsdt));
    initialize_header(xsdt->header,
                      "XSDT",
                      static_cast<uint32_t>(sizeof(TestSdtHeader) +
                                            (include_fadt ? 2u : 1u) * sizeof(uint64_t)));
    xsdt->entries[0] = kMadtPhysical;
    xsdt->entries[1] = include_fadt ? kFadtPhysical : 0;
    finalize_sdt(xsdt->header);

    std::memset(madt, 0, sizeof(*madt));
    initialize_header(madt->header, "APIC", sizeof(*madt));
    madt->lapic_address = 0xFEE00000u;
    madt->flags = 1;
    finalize_sdt(madt->header);

    std::memset(dsdt, 0, sizeof(*dsdt));
    initialize_header(*dsdt, "DSDT", sizeof(*dsdt));
    finalize_sdt(*dsdt);

    if(include_fadt)
    {
        std::memset(fadt, 0, sizeof(*fadt));
        initialize_header(fadt->header, "FACP", sizeof(*fadt));
        fadt->sci_interrupt = 9;
        fadt->x_dsdt = kDsdtPhysical;
        finalize_sdt(fadt->header);
    }
}

class AcpicaIntegration : public ::testing::Test
{
protected:
    void SetUp() override
    {
        acpica_reset_for_tests();
    }

    void TearDown() override
    {
        acpica_reset_for_tests();
    }
};
}  // namespace

TEST_F(AcpicaIntegration, InitializesTableManagerFromBootRsdp)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    build_tables(arena, true, false);

    PageFrameContainer frames = make_frames();
    VirtualMemory vm(frames);

    ASSERT_TRUE(acpica_initialize_tables(vm, make_boot_info(kRsdpPhysical)));
    EXPECT_TRUE(acpica_tables_initialized());
    EXPECT_STREQ("AE_OK", acpica_last_status());

    char fadt_signature[] = "FACP";
    ACPI_TABLE_HEADER* fadt = nullptr;
    ASSERT_EQ(AE_OK, AcpiGetTable(fadt_signature, 1, &fadt));
    ASSERT_NE(nullptr, fadt);
    EXPECT_EQ(0, std::memcmp(fadt->Signature, "FACP", 4));
    AcpiPutTable(fadt);

    char madt_signature[] = "APIC";
    ACPI_TABLE_HEADER* madt = nullptr;
    ASSERT_EQ(AE_OK, AcpiGetTable(madt_signature, 1, &madt));
    ASSERT_NE(nullptr, madt);
    EXPECT_EQ(0, std::memcmp(madt->Signature, "APIC", 4));
    AcpiPutTable(madt);
}

TEST_F(AcpicaIntegration, RejectsMissingBootRsdp)
{
    os1::host_test::PhysicalMemoryArena arena(kArenaBytes);
    build_tables(arena, true, false);

    PageFrameContainer frames = make_frames();
    VirtualMemory vm(frames);

    EXPECT_FALSE(acpica_initialize_tables(vm, make_boot_info(0)));
    EXPECT_FALSE(acpica_tables_initialized());
    EXPECT_STREQ("AE_NOT_FOUND", acpica_last_status());
}

TEST_F(AcpicaIntegration, RejectsBrokenOrIncompleteFirmwareTables)
{
    os1::host_test::PhysicalMemoryArena bad_checksum_arena(kArenaBytes);
    build_tables(bad_checksum_arena, true, true);

    PageFrameContainer frames = make_frames();
    VirtualMemory vm(frames);

    EXPECT_FALSE(acpica_initialize_tables(vm, make_boot_info(kRsdpPhysical)));
    EXPECT_FALSE(acpica_tables_initialized());

    os1::host_test::PhysicalMemoryArena missing_fadt_arena(kArenaBytes);
    build_tables(missing_fadt_arena, false, false);

    EXPECT_FALSE(acpica_initialize_tables(vm, make_boot_info(kRsdpPhysical)));
    EXPECT_FALSE(acpica_tables_initialized());
}