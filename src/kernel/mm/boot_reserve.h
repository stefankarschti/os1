// Boot-time physical range reservation helpers. These functions keep boot
// modules and display memory pinned until richer lifetime management exists.
#ifndef OS1_KERNEL_MM_BOOT_RESERVE_H
#define OS1_KERNEL_MM_BOOT_RESERVE_H

#include <stdint.h>

#include "handoff/bootinfo.h"

class PageFrameContainer;

// Return the byte length of a boot framebuffer from pitch and height metadata.
[[nodiscard]] uint64_t BootFramebufferLengthBytes(const BootFramebufferInfo &framebuffer);

// Reserve a clamped physical range in the page-frame allocator.
void ReserveTrackedPhysicalRange(PageFrameContainer &frames, uint64_t physical_start, uint64_t length);

#endif // OS1_KERNEL_MM_BOOT_RESERVE_H