// Shared power-of-two alignment helpers for boot-time physical mappings and
// range reservations. These helpers assume `alignment` is a non-zero power of
// two, matching all current page-size and table-alignment call sites.
#ifndef OS1_KERNEL_UTIL_ALIGN_H
#define OS1_KERNEL_UTIL_ALIGN_H

#include <stdint.h>

// Round `value` down to the nearest `alignment` boundary.
[[nodiscard]] inline uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
	return value & ~(alignment - 1);
}

// Round `value` up to the nearest `alignment` boundary.
[[nodiscard]] inline uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

#endif // OS1_KERNEL_UTIL_ALIGN_H