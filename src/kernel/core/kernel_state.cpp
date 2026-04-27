// Definitions for the narrow set of global kernel objects that still span
// multiple modules after the source-tree split.
#include "core/kernel_state.hpp"

Interrupts interrupts;
PageFrameContainer page_frames;
Keyboard keyboard(interrupts);

Terminal terminal[kNumTerminals];
Terminal *active_terminal = nullptr;

const BootInfo *g_boot_info = nullptr;
uint64_t g_kernel_root_cr3 = 0;
uint64_t g_timer_ticks = 0;
TextDisplayBackend *g_text_display = nullptr;