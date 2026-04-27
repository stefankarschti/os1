// Blocking console-read syscall helpers. These functions complete immediately
// when a line is queued or put the current thread to sleep until input arrives.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/page_frame.hpp"
#include "proc/thread.hpp"

// Try to satisfy a read syscall into user memory, possibly blocking `thread`.
bool try_complete_console_read(PageFrameContainer &frames,
		Thread *thread,
		uint64_t user_buffer,
		size_t length,
		long &result);
// Wake console-read waiters after input arrives.
void wake_console_readers(PageFrameContainer &frames);

