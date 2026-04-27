// Kernel console stream service. This layer mirrors bytes to serial debug and
// the active logical terminal without exposing terminal internals to syscalls.
#ifndef OS1_KERNEL_CONSOLE_CONSOLE_H
#define OS1_KERNEL_CONSOLE_CONSOLE_H

#include <stddef.h>

// Write exactly `length` bytes to the kernel console stream.
void WriteConsoleBytes(const char *data, size_t length);

// Write a nul-terminated line and append a newline on both console backends.
void WriteConsoleLine(const char *text);

#endif // OS1_KERNEL_CONSOLE_CONSOLE_H