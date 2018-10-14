#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sysinfo.h"
#include "terminal.h"
#include "interrupt.h"

void kernel_main(system_info *pinfo)
{
	system_info info = *pinfo;
	Terminal term((uint16_t*)0xB8000, 25, 80);
	term.move(info.cursory, info.cursorx);
	term.write("[elf_kernel64] hello 2++!\n");
	term.write("[elf_kernel64] setting up interrupts\n");
	idt_init();
	term.write("[elf_kernel64] still alive\n");
}
