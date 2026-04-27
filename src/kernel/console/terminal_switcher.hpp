// Terminal hotkey policy for the fixed kernel terminal set. Hardware keyboard
// code reports raw scancodes here; this module decides whether they select a
// different logical terminal.
#pragma once

#include <stdint.h>

// Handle a raw keyboard scancode and return whether normal character handling
// should continue after terminal-switch processing.
bool kernel_keyboard_hook(uint16_t scancode);
