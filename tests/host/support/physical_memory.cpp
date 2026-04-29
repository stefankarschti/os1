#include "support/physical_memory.hpp"

#include <stdlib.h>

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
    : physical_base_(physical_base), data_(size_bytes, 0)
{
    clear_physical_memory_ranges();
    register_physical_memory_range(physical_base_, data_.data(), data_.size());
}

PhysicalMemoryArena::~PhysicalMemoryArena()
{
    clear_physical_memory_ranges();
}

uint64_t PhysicalMemoryArena::physical_base() const
{
    return physical_base_;
}

size_t PhysicalMemoryArena::size() const
{
    return data_.size();
}

uint8_t* PhysicalMemoryArena::data()
{
    return data_.data();
}

const uint8_t* PhysicalMemoryArena::data() const
{
    return data_.data();
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
