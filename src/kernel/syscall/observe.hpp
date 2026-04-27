// Read-only observability syscall. It snapshots kernel state into stable UAPI
// structs without giving user space direct pointers to kernel-owned records.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drivers/display/text_display.hpp"
#include "handoff/boot_info.hpp"
#include "mm/page_frame.hpp"

struct ObserveContext
{
    const BootInfo* boot_info = nullptr;
    const TextDisplayBackend* text_display = nullptr;
    uint64_t timer_ticks = 0;
    PageFrameContainer* frames = nullptr;
};

// Fill an observe record of `kind` into a user buffer.
long sys_observe(const ObserveContext& context, uint64_t kind, uint64_t user_buffer, size_t length);
