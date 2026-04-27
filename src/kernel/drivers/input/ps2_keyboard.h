// PS/2 keyboard driver. It reports decoded character input into console_input
// and raw scancodes into console terminal-switch policy.
#ifndef OS1_KERNEL_DRIVERS_INPUT_PS2_KEYBOARD_H
#define OS1_KERNEL_DRIVERS_INPUT_PS2_KEYBOARD_H

#include "stdint.h"
#include "stddef.h"

#include "arch/x86_64/interrupt/interrupt.h"
#include "console/terminal.h"

class Keyboard
{
public:
	// Capture the interrupt table used to register the IRQ1 callback.
	Keyboard(Interrupts& ints)
		:interrupts_(ints) {}
	// Register the keyboard IRQ handler.
	bool Initialize();
	// Keep a reference to the visible terminal for legacy keypress behavior.
	void SetActiveTerminal(Terminal* terminal);

private:
	Interrupts &interrupts_;
	Terminal *active_terminal_ = nullptr;
	// IRQ1 callback that drains the controller and forwards usable input.
	static void IRQHandler(Keyboard* object);
};

#endif // OS1_KERNEL_DRIVERS_INPUT_PS2_KEYBOARD_H

