// 8253/8254 PIT setup used by the current timer IRQ source.
#include "drivers/timer/pit.h"

#include "arch/x86_64/cpu/io_port.h"

uint16_t SetTimer(uint16_t frequency)
{
	uint32_t divisor = 1193180 / frequency;
	if(divisor > 65536)
	{
		divisor = 65536;
	}
	outb(0x43, 0x34);
	outb(0x40, divisor & 0xFF);
	outb(0x40, (divisor >> 8) & 0xFF);
	return 1193180 / divisor;
}