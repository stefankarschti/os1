#pragma once

#include <stddef.h>
#include <stdint.h>

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
    size_t size_bytes_ = 0;
    uint8_t* data_ = nullptr;
};
}  // namespace os1::host_test
