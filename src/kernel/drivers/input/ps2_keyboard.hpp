// PS/2 keyboard driver. It reports decoded character input into console_input
// and raw scancodes into console terminal-switch policy.
#pragma once

#include "arch/x86_64/interrupt/interrupt.hpp"
#include "console/terminal.hpp"
#include "stddef.h"
#include "stdint.h"

class Keyboard
{
public:
    // Capture the interrupt table used to register the IRQ1 callback.
    Keyboard(Interrupts& ints) : interrupts_(ints) {}
    // Register the keyboard IRQ handler.
    bool initialize();
    // Keep a reference to the visible terminal for legacy keypress behavior.
    void set_active_terminal(Terminal* terminal);

private:
    Interrupts& interrupts_;
    Terminal* active_terminal_ = nullptr;
    // IRQ1 callback that drains the controller and forwards usable input.
    static void irq_handler(Keyboard* object);
};
