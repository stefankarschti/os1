// Canonical console-input buffer shared by serial polling, PS/2 keyboard input,
// and blocking read syscalls.
#pragma once

#include <stddef.h>

#include "sync/smp.hpp"

struct WaitQueue;

constexpr size_t kConsoleInputMaxLineBytes = 128;

extern Spinlock g_console_input_lock;

// Reset input queues and current line state.
void console_input_initialize();
// Append one decoded keyboard character to the canonical line buffer.
void console_input_on_keyboard_char(char ascii);
// Poll the serial port and feed any pending characters into console input.
void console_input_poll_serial();
// Return true when a complete line is available for readers.
bool console_input_has_line();
// Pop the oldest complete line into `buffer`.
bool console_input_pop_line(char* buffer, size_t buffer_size, size_t& line_length);
// Return the wait queue used by blocking console-read syscalls.
WaitQueue& console_input_read_wait_queue();
