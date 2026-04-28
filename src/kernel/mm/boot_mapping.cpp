// Identity-mapping policy for boot-critical physical ranges.
#include "mm/boot_mapping.hpp"

#include "handoff/memory_layout.h"
#include "mm/virtual_memory.hpp"
#include "util/align.hpp"

bool map_bootstrap_identity_range(VirtualMemory& vm, uint64_t physical_start, uint64_t length)
{
    if(0 == length)
    {
        return true;
    }

    const uint64_t start = align_down(physical_start, kPageSize);
    const uint64_t end = align_up(physical_start + length, kPageSize);
    return vm.map_physical(
        start, start, (end - start) / kPageSize, PageFlags::Present | PageFlags::Write);
}

bool map_direct_range(VirtualMemory& vm, uint64_t physical_start, uint64_t length)
{
    if(0 == length)
    {
        return true;
    }

    const uint64_t start = align_down(physical_start, kPageSize);
    const uint64_t end = align_up(physical_start + length, kPageSize);
    return vm.map_physical(phys_to_virt(start),
                           start,
                           (end - start) / kPageSize,
                           PageFlags::Present | PageFlags::Write);
}

bool map_mmio_range(VirtualMemory& vm, uint64_t physical_start, uint64_t length)
{
    return map_direct_range(vm, physical_start, length);
}