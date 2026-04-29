#pragma once

#include <stdint.h>

namespace limine_shim
{
void init_serial();
void write_serial_char(char value);
void write_serial(const char* text);
void write_serial_ln(const char* text);
void write_serial_hex(uint64_t value);
}  // namespace limine_shim