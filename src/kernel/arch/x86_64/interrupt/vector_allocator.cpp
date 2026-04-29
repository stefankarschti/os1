// Simple fixed-range vector allocator. The first MSI/MSI-X work stays BSP-only,
// so this allocator follows the same contract for now.
#include "arch/x86_64/interrupt/vector_allocator.hpp"

#include <stddef.h>

#include "sync/smp.hpp"

namespace
{
constexpr size_t kDynamicIrqVectorCount =
    static_cast<size_t>(kDynamicIrqVectorLimit - kDynamicIrqVectorBase + 1u);
constexpr size_t kVectorBitmapWordBits = 64;
constexpr size_t kVectorBitmapWordCount =
    (kDynamicIrqVectorCount + kVectorBitmapWordBits - 1u) / kVectorBitmapWordBits;

uint64_t g_vector_bitmap[kVectorBitmapWordCount]{};

[[nodiscard]] bool bitmap_get(size_t index)
{
    return (g_vector_bitmap[index / kVectorBitmapWordBits] &
            (1ull << (index % kVectorBitmapWordBits))) != 0;
}

void bitmap_set(size_t index)
{
    g_vector_bitmap[index / kVectorBitmapWordBits] |= (1ull << (index % kVectorBitmapWordBits));
}

void bitmap_clear(size_t index)
{
    g_vector_bitmap[index / kVectorBitmapWordBits] &= ~(1ull << (index % kVectorBitmapWordBits));
}

[[nodiscard]] size_t vector_index(uint8_t vector)
{
    return static_cast<size_t>(vector - kDynamicIrqVectorBase);
}
}  // namespace

void irq_vector_allocator_reset()
{
    KASSERT_ON_BSP();
    for(size_t i = 0; i < kVectorBitmapWordCount; ++i)
    {
        g_vector_bitmap[i] = 0;
    }
}

bool irq_vector_is_allocatable(uint8_t vector)
{
    return (vector >= kDynamicIrqVectorBase) && (vector <= kDynamicIrqVectorLimit) &&
           (vector != T_SYSCALL);
}

bool irq_vector_is_allocated(uint8_t vector)
{
    if(!irq_vector_is_allocatable(vector))
    {
        return false;
    }
    return bitmap_get(vector_index(vector));
}

bool irq_allocate_vector(uint8_t& vector)
{
    KASSERT_ON_BSP();
    for(uint16_t candidate = kDynamicIrqVectorBase; candidate <= kDynamicIrqVectorLimit;
        ++candidate)
    {
        const uint8_t current = static_cast<uint8_t>(candidate);
        if(!irq_vector_is_allocatable(current) || irq_vector_is_allocated(current))
        {
            continue;
        }
        bitmap_set(vector_index(current));
        vector = current;
        return true;
    }
    return false;
}

bool irq_reserve_vector(uint8_t vector)
{
    KASSERT_ON_BSP();
    if(!irq_vector_is_allocatable(vector) || irq_vector_is_allocated(vector))
    {
        return false;
    }
    bitmap_set(vector_index(vector));
    return true;
}

bool irq_free_vector(uint8_t vector)
{
    KASSERT_ON_BSP();
    if(!irq_vector_is_allocatable(vector) || !irq_vector_is_allocated(vector))
    {
        return false;
    }
    bitmap_clear(vector_index(vector));
    return true;
}
