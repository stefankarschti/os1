// Shared kernel root state used while the early monolithic bring-up sequence is
// being split into focused modules. This header is intentionally small: every
// symbol here is a process-wide singleton that does not yet have a narrower
// owner without changing boot behavior.
#ifndef OS1_KERNEL_CORE_KERNEL_STATE_H
#define OS1_KERNEL_CORE_KERNEL_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/interrupt/interrupt.h"
#include "console/terminal.h"
#include "drivers/display/text_display.h"
#include "drivers/input/ps2_keyboard.h"
#include "handoff/bootinfo.h"
#include "mm/page_frame.h"

// Kernel-wide interrupt descriptor table and IRQ callback registry.
extern Interrupts interrupts;

// Physical page allocator used by VM, process, syscall, and driver code.
extern PageFrameContainer page_frames;

// PS/2 keyboard device instance wired to the shared interrupt table.
extern Keyboard keyboard;

// The kernel keeps a small fixed terminal set until sessions/PTYs exist.
inline constexpr size_t kNumTerminals = 12;
extern Terminal terminal[kNumTerminals];
extern Terminal *active_terminal;

// Owned boot contract copied out of bootloader staging memory.
extern const BootInfo *g_boot_info;

// Active kernel page-table root used for syscall CR3 transitions.
extern uint64_t g_kernel_root_cr3;

// Monotonic PIT tick counter surfaced through the observe syscall.
extern uint64_t g_timer_ticks;

// Selected text output backend for terminal presentation and observability.
extern TextDisplayBackend *g_text_display;

#endif // OS1_KERNEL_CORE_KERNEL_STATE_H