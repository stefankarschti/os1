#include "support/physical_memory.hpp"

#include "handoff/memory_layout.h"

#include <stdlib.h>

#include <string.h>

#include <vector>

namespace
{
struct PhysicalRange
{
    uint64_t physical_start;
    uint8_t* host_start;
    size_t length;
};

std::vector<PhysicalRange>& ranges()
{
    static std::vector<PhysicalRange> physical_ranges;
    return physical_ranges;
}
}  // namespace

namespace os1::host_test
{
void clear_physical_memory_ranges()
{
    ranges().clear();
}

void register_physical_memory_range(uint64_t physical_start, void* host_start, size_t length)
{
    ranges().push_back(PhysicalRange{
        .physical_start = physical_start,
        .host_start = static_cast<uint8_t*>(host_start),
        .length = length,
    });
}

PhysicalMemoryArena::PhysicalMemoryArena(size_t size_bytes, uint64_t physical_base)
    : physical_base_(physical_base), size_bytes_(size_bytes)
{
    void* allocation = nullptr;
    if((0 == size_bytes_) || (0 != posix_memalign(&allocation, kPageSize, size_bytes_)))
    {
        abort();
    }

    data_ = static_cast<uint8_t*>(allocation);
    memset(data_, 0, size_bytes_);

    clear_physical_memory_ranges();
    register_physical_memory_range(physical_base_, data_, size_bytes_);
}

PhysicalMemoryArena::~PhysicalMemoryArena()
{
    clear_physical_memory_ranges();
    free(data_);
}

uint64_t PhysicalMemoryArena::physical_base() const
{
    return physical_base_;
}

size_t PhysicalMemoryArena::size() const
{
    return size_bytes_;
}

uint8_t* PhysicalMemoryArena::data()
{
    return data_;
}

const uint8_t* PhysicalMemoryArena::data() const
{
    return data_;
}
}  // namespace os1::host_test

void* os1_host_physical_pointer(uint64_t physical_address)
{
    for(const PhysicalRange& range : ranges())
    {
        if((physical_address >= range.physical_start) &&
           ((physical_address - range.physical_start) < range.length))
        {
            return range.host_start + (physical_address - range.physical_start);
        }
    }
    abort();
}
