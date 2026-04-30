#include "platform/pci_msi.hpp"

#include <gtest/gtest.h>

TEST(PciMsi, BuildsX86MessageAddressAndData)
{
    uint64_t address = 0;
    uint32_t data = 0;
    pci_build_msi_message(0x2A, 0x53, address, data);

    EXPECT_EQ(0xFEE2A000ull, address);
    EXPECT_EQ(0x53u, data);
}

TEST(PciMsi, WritesMsixTableEntry)
{
    alignas(16) uint32_t table[8]{};
    pci_msix_write_table_entry(table, 1, 0x123456789ABCDEF0ull, 0x55AAu, false);

    EXPECT_EQ(0x9ABCDEF0u, table[4]);
    EXPECT_EQ(0x12345678u, table[5]);
    EXPECT_EQ(0x55AAu, table[6]);
    EXPECT_EQ(0u, table[7]);
}
