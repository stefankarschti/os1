// Definitions for the narrow set of global kernel objects that still span
// multiple modules after the source-tree split.
#include "core/kernel_state.hpp"

Interrupts interrupts;
OS1_BSP_ONLY PageFrameContainer page_frames;
Keyboard keyboard(interrupts);

// BSP-only for now: terminal state is mutated only by the BSP console/input path.
OS1_BSP_ONLY Terminal terminal[kNumTerminals];
OS1_BSP_ONLY Terminal* active_terminal = nullptr;

const BootInfo* g_boot_info = nullptr;
uint64_t g_kernel_root_cr3 = 0;
bool g_kernel_direct_map_ready = false;
uint64_t g_timer_ticks = 0;
TextDisplayBackend* g_text_display = nullptr;
