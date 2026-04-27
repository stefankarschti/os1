// Canonical console-input buffer shared by serial polling, PS/2 keyboard input,
// and blocking read syscalls.
#ifndef OS1_KERNEL_CONSOLE_CONSOLE_INPUT_H
#define OS1_KERNEL_CONSOLE_CONSOLE_INPUT_H

#include <stddef.h>

constexpr size_t kConsoleInputMaxLineBytes = 128;

// Reset input queues and current line state.
void ConsoleInputInitialize();
// Append one decoded keyboard character to the canonical line buffer.
void ConsoleInputOnKeyboardChar(char ascii);
// Poll the serial port and feed any pending characters into console input.
void ConsoleInputPollSerial();
// Return true when a complete line is available for readers.
bool ConsoleInputHasLine();
// Pop the oldest complete line into `buffer`.
bool ConsoleInputPopLine(char *buffer, size_t buffer_size, size_t &line_length);

#endif // OS1_KERNEL_CONSOLE_CONSOLE_INPUT_H
