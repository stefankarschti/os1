#include "core/kernel_state.hpp"

PageFrameContainer page_frames;
Spinlock g_page_frames_lock{"page-frames"};