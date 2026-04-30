// USB HID boot-keyboard report decoder used by xHCI keyboard input.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace usb::hid
{
constexpr size_t kBootKeyboardReportBytes = 8u;
constexpr uint8_t kModifierLeftShift = 1u << 1;
constexpr uint8_t kModifierRightShift = 1u << 5;

struct BootKeyboardState
{
    uint8_t modifiers = 0;
    uint8_t keys[6]{};
};

char boot_key_usage_to_ascii(uint8_t usage, bool shift);
bool boot_keyboard_apply_report(BootKeyboardState& state,
                                const uint8_t* report,
                                size_t report_bytes,
                                char* output,
                                size_t output_capacity,
                                size_t& output_count);
}  // namespace usb::hid