// Direct-map-backed kernel small-object allocator.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.hpp"

enum class KmallocFlags : uint32_t
{
    None = 0,
    Zero = 1u << 0,
    NoGrow = 1u << 1,
    Atomic = 1u << 2,
    PanicOnFail = 1u << 3,
};

inline constexpr KmallocFlags operator|(KmallocFlags left, KmallocFlags right)
{
    return static_cast<KmallocFlags>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

inline constexpr KmallocFlags operator&(KmallocFlags left, KmallocFlags right)
{
    return static_cast<KmallocFlags>(static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
}

inline constexpr KmallocFlags& operator|=(KmallocFlags& left, KmallocFlags right)
{
    left = left | right;
    return left;
}

void kmem_init(PageFrameContainer& frames);
[[nodiscard]] void* kmalloc(size_t size, KmallocFlags flags = KmallocFlags::None);
void kfree(void* ptr);