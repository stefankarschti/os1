// Kernel console stream service. This layer mirrors bytes to serial debug and
// the active logical terminal without exposing terminal internals to syscalls.
#pragma once

#include <stddef.h>

// write exactly `length` bytes to the kernel console stream.
void write_console_bytes(const char *data, size_t length);

// write a nul-terminated line and append a newline on both console backends.
void write_console_line(const char *text);

