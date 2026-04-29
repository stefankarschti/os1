#pragma once

#include <stdint.h>

namespace limine_shim
{
constexpr uint64_t kPageSize = 0x1000;
constexpr uint64_t kPageMask = ~(kPageSize - 1);
constexpr uint64_t kTwoMiBPageSize = 0x200000;

void set_hhdm_offset(uint64_t offset);
[[nodiscard]] bool hhdm_ready();
[[nodiscard]] void* map_physical(uint64_t physical_address);

template<typename T>
[[nodiscard]] inline T* map_physical_pointer(uint64_t physical_address)
{
    return static_cast<T*>(map_physical(physical_address));
}

[[nodiscard]] bool translate_limine_virtual(uint64_t virtual_address, uint64_t& physical_address);

[[nodiscard]] inline bool translate_shim_pointer(const void* pointer, uint64_t& physical_address)
{
    return translate_limine_virtual(reinterpret_cast<uint64_t>(pointer), physical_address);
}

template<typename T>
[[nodiscard]] inline bool limine_pointer_mapped(const T* pointer)
{
    if(nullptr == pointer)
    {
        return false;
    }

    uint64_t physical_address = 0;
    return translate_shim_pointer(pointer, physical_address);
}

[[nodiscard]] bool ensure_bootstrap_low_window(uint64_t required_bytes);
[[nodiscard]] bool ensure_kernel_higher_half_window(uint64_t required_bytes);
}  // namespace limine_shim