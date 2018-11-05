#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include "stdint.h"
#include "stddef.h"

#include "interrupt.h"
#include "terminal.h"

/**
 * @brief The Keyboard class
 */
class Keyboard
{
public:
	Keyboard(Interrupts& ints)
		:interrupts_(ints) {}
	bool Initialize();
	void SetActiveTerminal(Terminal* terminal);

private:
	Interrupts &interrupts_;
	Terminal *active_terminal_ = nullptr;
	static void IRQHandler(Keyboard* object);
};

#endif

