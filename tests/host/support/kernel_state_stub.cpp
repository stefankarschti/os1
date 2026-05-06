#include "core/kernel_state.hpp"

constinit PageFrameContainer page_frames;
Spinlock g_page_frames_lock{"page-frames"};