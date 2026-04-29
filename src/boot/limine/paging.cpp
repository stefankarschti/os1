#include "paging.hpp"

#include "freestanding/string.hpp"
#include "handoff/memory_layout.h"
#include "util/align.hpp"

namespace limine_shim
{
namespace
{
constexpr uint64_t kMaxIdentityMapBytes = 512 * kTwoMiBPageSize;
constexpr uint64_t kHugePageBit = 1ull << 7;
constexpr uint64_t kPageEntryAddressMask = 0x000FFFFFFFFFF000ull;
constexpr uint64_t kOneGiBPageAddressMask = 0x000FFFFFC0000000ull;
constexpr uint64_t kTwoMiBPageAddressMask = 0x000FFFFFFFE00000ull;

alignas(kPageSize) constinit uint64_t g_bootstrap_low_pml3[512]{};
alignas(kPageSize) constinit uint64_t g_bootstrap_low_pml2[512]{};
alignas(kPageSize) constinit uint64_t g_kernel_high_pml3[512]{};
alignas(kPageSize) constinit uint64_t g_kernel_high_pml2[512]{};
constinit uint64_t g_hhdm_offset = 0;
constinit bool g_hhdm_offset_valid = false;

[[nodiscard]] uint64_t read_cr3()
{
    uint64_t value = 0;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

[[nodiscard]] uint64_t page_index(uint64_t virtual_address, unsigned shift)
{
    return (virtual_address >> shift) & 0x1FFull;
}

[[nodiscard]] const uint64_t* map_physical_table(uint64_t physical_address)
{
    return map_physical_pointer<const uint64_t>(physical_address);
}

[[nodiscard]] bool limine_mapping_matches(uint64_t virtual_start,
                                          uint64_t physical_start,
                                          uint64_t length)
{
    if(0 == length)
    {
        return true;
    }

    uint64_t translated = 0;
    if(!translate_limine_virtual(virtual_start, translated) || (translated != physical_start))
    {
        return false;
    }

    const uint64_t last_offset = length - 1;
    if(!translate_limine_virtual(virtual_start + last_offset, translated) ||
       (translated != (physical_start + last_offset)))
    {
        return false;
    }

    return true;
}

void reload_cr3()
{
    const uint64_t value = read_cr3();
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}
}  // namespace

void set_hhdm_offset(uint64_t offset)
{
    g_hhdm_offset = offset;
    g_hhdm_offset_valid = true;
}

[[nodiscard]] bool hhdm_ready()
{
    return g_hhdm_offset_valid;
}

[[nodiscard]] void* map_physical(uint64_t physical_address)
{
    if(!g_hhdm_offset_valid)
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(physical_address + g_hhdm_offset);
}

[[nodiscard]] bool translate_limine_virtual(uint64_t virtual_address, uint64_t& physical_address)
{
    const uint64_t* pml4 = map_physical_table(read_cr3() & kPageMask);
    if(nullptr == pml4)
    {
        return false;
    }

    const uint64_t pml4e = pml4[page_index(virtual_address, 39)];
    if(0 == (pml4e & 1ull))
    {
        return false;
    }

    const uint64_t* pml3 = map_physical_table(pml4e & kPageEntryAddressMask);
    if(nullptr == pml3)
    {
        return false;
    }
    const uint64_t pml3e = pml3[page_index(virtual_address, 30)];
    if(0 == (pml3e & 1ull))
    {
        return false;
    }
    if(0 != (pml3e & kHugePageBit))
    {
        physical_address =
            (pml3e & kOneGiBPageAddressMask) | (virtual_address & ((1ull << 30) - 1ull));
        return true;
    }

    const uint64_t* pml2 = map_physical_table(pml3e & kPageEntryAddressMask);
    if(nullptr == pml2)
    {
        return false;
    }
    const uint64_t pml2e = pml2[page_index(virtual_address, 21)];
    if(0 == (pml2e & 1ull))
    {
        return false;
    }
    if(0 != (pml2e & kHugePageBit))
    {
        physical_address =
            (pml2e & kTwoMiBPageAddressMask) | (virtual_address & ((1ull << 21) - 1ull));
        return true;
    }

    const uint64_t* pml1 = map_physical_table(pml2e & kPageEntryAddressMask);
    if(nullptr == pml1)
    {
        return false;
    }
    const uint64_t pml1e = pml1[page_index(virtual_address, 12)];
    if(0 == (pml1e & 1ull))
    {
        return false;
    }

    physical_address = (pml1e & kPageEntryAddressMask) | (virtual_address & 0xFFFull);
    return true;
}

[[nodiscard]] bool ensure_bootstrap_low_window(uint64_t required_bytes)
{
    const uint64_t mapped_bytes = align_up(required_bytes, kTwoMiBPageSize);
    if((0 == mapped_bytes) || (mapped_bytes > kMaxIdentityMapBytes))
    {
        return false;
    }

    uint64_t* pml4 = map_physical_pointer<uint64_t>(read_cr3() & kPageMask);
    if(nullptr == pml4)
    {
        return false;
    }

    uint64_t pml3_physical = 0;
    uint64_t* pml3 = nullptr;
    if(0 == (pml4[0] & 1ull))
    {
        freestanding::zero_bytes(g_bootstrap_low_pml3, sizeof(g_bootstrap_low_pml3));
        if(!translate_shim_pointer(g_bootstrap_low_pml3, pml3_physical))
        {
            return false;
        }
        pml4[0] = pml3_physical | 0x3ull;
        pml3 = g_bootstrap_low_pml3;
    }
    else
    {
        pml3_physical = pml4[0] & kPageEntryAddressMask;
        pml3 = map_physical_pointer<uint64_t>(pml3_physical);
        if(nullptr == pml3)
        {
            return false;
        }
    }

    if(0 != (pml3[0] & kHugePageBit))
    {
        return limine_mapping_matches(0, 0, mapped_bytes);
    }

    uint64_t pml2_physical = 0;
    uint64_t* pml2 = nullptr;
    if(0 == (pml3[0] & 1ull))
    {
        freestanding::zero_bytes(g_bootstrap_low_pml2, sizeof(g_bootstrap_low_pml2));
        if(!translate_shim_pointer(g_bootstrap_low_pml2, pml2_physical))
        {
            return false;
        }
        pml3[0] = pml2_physical | 0x3ull;
        pml2 = g_bootstrap_low_pml2;
    }
    else
    {
        pml2_physical = pml3[0] & kPageEntryAddressMask;
        pml2 = map_physical_pointer<uint64_t>(pml2_physical);
        if(nullptr == pml2)
        {
            return false;
        }
    }

    const uint64_t page_count = mapped_bytes / kTwoMiBPageSize;
    for(uint64_t i = 0; i < page_count; ++i)
    {
        const uint64_t physical_base = i * kTwoMiBPageSize;
        if(0 != (pml2[i] & 1ull))
        {
            if(!limine_mapping_matches(physical_base, physical_base, kTwoMiBPageSize))
            {
                return false;
            }
        }
        else
        {
            pml2[i] = physical_base | 0x83ull;
        }
    }

    reload_cr3();
    return true;
}

[[nodiscard]] bool ensure_kernel_higher_half_window(uint64_t required_bytes)
{
    const uint64_t mapped_bytes = align_up(required_bytes, kTwoMiBPageSize);
    if((0 == mapped_bytes) || (mapped_bytes > kMaxIdentityMapBytes))
    {
        return false;
    }

    uint64_t* pml4 = map_physical_pointer<uint64_t>(read_cr3() & kPageMask);
    if(nullptr == pml4)
    {
        return false;
    }

    const uint64_t kernel_pml3_index = page_index(kKernelVirtualOffset, 30);
    uint64_t pml3_physical = 0;
    uint64_t* pml3 = nullptr;
    if(0 == (pml4[kKernelPml4Index] & 1ull))
    {
        freestanding::zero_bytes(g_kernel_high_pml3, sizeof(g_kernel_high_pml3));
        if(!translate_shim_pointer(g_kernel_high_pml3, pml3_physical))
        {
            return false;
        }
        pml4[kKernelPml4Index] = pml3_physical | 0x3ull;
        pml3 = g_kernel_high_pml3;
    }
    else
    {
        pml3_physical = pml4[kKernelPml4Index] & kPageEntryAddressMask;
        pml3 = map_physical_pointer<uint64_t>(pml3_physical);
        if(nullptr == pml3)
        {
            return false;
        }
    }

    if(0 != (pml3[kernel_pml3_index] & kHugePageBit))
    {
        return limine_mapping_matches(kKernelVirtualOffset, 0, mapped_bytes);
    }

    uint64_t pml2_physical = 0;
    uint64_t* pml2 = nullptr;
    if(0 == (pml3[kernel_pml3_index] & 1ull))
    {
        freestanding::zero_bytes(g_kernel_high_pml2, sizeof(g_kernel_high_pml2));
        if(!translate_shim_pointer(g_kernel_high_pml2, pml2_physical))
        {
            return false;
        }
        pml3[kernel_pml3_index] = pml2_physical | 0x3ull;
        pml2 = g_kernel_high_pml2;
    }
    else
    {
        pml2_physical = pml3[kernel_pml3_index] & kPageEntryAddressMask;
        pml2 = map_physical_pointer<uint64_t>(pml2_physical);
        if(nullptr == pml2)
        {
            return false;
        }
    }

    const uint64_t page_count = mapped_bytes / kTwoMiBPageSize;
    for(uint64_t i = 0; i < page_count; ++i)
    {
        const uint64_t physical_base = i * kTwoMiBPageSize;
        const uint64_t virtual_base = kKernelVirtualOffset + physical_base;
        if(0 != (pml2[i] & 1ull))
        {
            if(!limine_mapping_matches(virtual_base, physical_base, kTwoMiBPageSize))
            {
                return false;
            }
        }
        else
        {
            pml2[i] = physical_base | 0x83ull;
        }
    }

    reload_cr3();
    return true;
}
}  // namespace limine_shim