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
	bool Initialize();
	void SetActiveTerminal(Terminal* terminal);

private:
	Terminal *active_terminal = nullptr;
	static void IRQHandler(Keyboard* object);
};

#endif

