// Early boot identity-mapping helpers. These are intentionally separate from
// the general VirtualMemory class because they encode bring-up policy: map a
// physical device or handoff range at the same virtual address before CR3 flips.
#pragma once

#include <stdint.h>

class VirtualMemory;

// Map the physical byte range `[physical_start, physical_start + length)` into
// the same virtual addresses with supervisor read/write permissions.
bool map_identity_range(VirtualMemory& vm, uint64_t physical_start, uint64_t length);
