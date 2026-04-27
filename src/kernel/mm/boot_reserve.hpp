// Boot-time physical range reservation helpers. These functions keep boot
// modules and display memory pinned until richer lifetime management exists.
#pragma once

#include <stdint.h>

#include "handoff/boot_info.hpp"

class PageFrameContainer;

// Return the byte length of a boot framebuffer from pitch and height metadata.
[[nodiscard]] uint64_t BootFramebufferLengthBytes(const BootFramebufferInfo& framebuffer);

// Reserve a clamped physical range in the page-frame allocator.
void reserve_tracked_physical_range(PageFrameContainer& frames,
                                    uint64_t physical_start,
                                    uint64_t length);
