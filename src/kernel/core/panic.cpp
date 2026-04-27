// Minimal halt loop used after unrecoverable kernel failures.
#include "core/panic.h"

[[noreturn]] void HaltForever()
{
	for(;;)
	{
		asm volatile("cli");
		asm volatile("hlt");
	}
}