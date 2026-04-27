// Terminal hotkey policy for the fixed kernel terminal set. Hardware keyboard
// code reports raw scancodes here; this module decides whether they select a
// different logical terminal.
#ifndef OS1_KERNEL_CONSOLE_TERMINAL_SWITCHER_H
#define OS1_KERNEL_CONSOLE_TERMINAL_SWITCHER_H

#include <stdint.h>

// Handle a raw keyboard scancode and return whether normal character handling
// should continue after terminal-switch processing.
bool KernelKeyboardHook(uint16_t scancode);

#endif // OS1_KERNEL_CONSOLE_TERMINAL_SWITCHER_H