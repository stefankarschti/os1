#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace os1::host_test
{
void clear_physical_memory_ranges();
void register_physical_memory_range(uint64_t physical_start, void* host_start, size_t length);

class PhysicalMemoryArena
{
public:
    explicit PhysicalMemoryArena(size_t size_bytes, uint64_t physical_base = 0);
    ~PhysicalMemoryArena();

    PhysicalMemoryArena(const PhysicalMemoryArena&) = delete;
    PhysicalMemoryArena& operator=(const PhysicalMemoryArena&) = delete;

    uint64_t physical_base() const;
    size_t size() const;
    uint8_t* data();
    const uint8_t* data() const;

private:
    uint64_t physical_base_;
    std::vector<uint8_t> data_;
};
}  // namespace os1::host_test
