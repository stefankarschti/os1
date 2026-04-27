// Early boot identity-mapping helpers. These are intentionally separate from
// the general VirtualMemory class because they encode bring-up policy: map a
// physical device or handoff range at the same virtual address before CR3 flips.
#ifndef OS1_KERNEL_MM_BOOT_MAPPING_H
#define OS1_KERNEL_MM_BOOT_MAPPING_H

#include <stdint.h>

class VirtualMemory;

// Map the physical byte range `[physical_start, physical_start + length)` into
// the same virtual addresses with supervisor read/write permissions.
bool MapIdentityRange(VirtualMemory &vm, uint64_t physical_start, uint64_t length);

#endif // OS1_KERNEL_MM_BOOT_MAPPING_H