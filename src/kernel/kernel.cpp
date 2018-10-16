#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sysinfo.h"
#include "terminal.h"
#include "interrupt.h"
#include "memory.h"
#include "../libc/stdlib.h"

static Terminal *gTerminal = nullptr;

unsigned short plain_map[256] = {
	0xf200,	0xf01b,	0xf031,	0xf032,	0xf033,	0xf034,	0xf035,	0xf036,
	0xf037,	0xf038,	0xf039,	0xf030,	0xf02d,	0xf03d,	0xf07f,	0xf009,
	0xfb71,	0xfb77,	0xfb65,	0xfb72,	0xfb74,	0xfb79,	0xfb75,	0xfb69,
	0xfb6f,	0xfb70,	0xf05b,	0xf05d,	0xf20a,	0xf702,	0xfb61,	0xfb73,
	0xfb64,	0xfb66,	0xfb67,	0xfb68,	0xfb6a,	0xfb6b,	0xfb6c,	0xf03b,
	0xf027,	0xf060,	0xf700,	0xf05c,	0xfb7a,	0xfb78,	0xfb63,	0xfb76,
	0xfb62,	0xfb6e,	0xfb6d,	0xf02c,	0xf02e,	0xf02f,	0xf700,	0xf30c,
	0xf703,	0xf020,	0xf207,	0xf100,	0xf101,	0xf102,	0xf103,	0xf104,
	0xf105,	0xf106,	0xf107,	0xf108,	0xf109,	0xf208,	0xf209,	0xf307,
	0xf308,	0xf309,	0xf30b,	0xf304,	0xf305,	0xf306,	0xf30a,	0xf301,
	0xf302,	0xf303,	0xf300,	0xf310,	0xf206,	0xf200,	0xf03c,	0xf10a,
	0xf10b,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf30e,	0xf702,	0xf30d,	0xf01c,	0xf701,	0xf205,	0xf114,	0xf603,
	0xf118,	0xf601,	0xf602,	0xf117,	0xf600,	0xf119,	0xf115,	0xf116,
	0xf11a,	0xf10c,	0xf10d,	0xf11b,	0xf11c,	0xf110,	0xf311,	0xf11d,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
};

unsigned short shift_map[256] = {
	0xf200,	0xf01b,	0xf021,	0xf040,	0xf023,	0xf024,	0xf025,	0xf05e,
	0xf026,	0xf02a,	0xf028,	0xf029,	0xf05f,	0xf02b,	0xf07f,	0xf009,
	0xfb51,	0xfb57,	0xfb45,	0xfb52,	0xfb54,	0xfb59,	0xfb55,	0xfb49,
	0xfb4f,	0xfb50,	0xf07b,	0xf07d,	0xf201,	0xf702,	0xfb41,	0xfb53,
	0xfb44,	0xfb46,	0xfb47,	0xfb48,	0xfb4a,	0xfb4b,	0xfb4c,	0xf03a,
	0xf022,	0xf07e,	0xf700,	0xf07c,	0xfb5a,	0xfb58,	0xfb43,	0xfb56,
	0xfb42,	0xfb4e,	0xfb4d,	0xf03c,	0xf03e,	0xf03f,	0xf700,	0xf30c,
	0xf703,	0xf020,	0xf207,	0xf10a,	0xf10b,	0xf10c,	0xf10d,	0xf10e,
	0xf10f,	0xf110,	0xf111,	0xf112,	0xf113,	0xf213,	0xf203,	0xf307,
	0xf308,	0xf309,	0xf30b,	0xf304,	0xf305,	0xf306,	0xf30a,	0xf301,
	0xf302,	0xf303,	0xf300,	0xf310,	0xf206,	0xf200,	0xf03e,	0xf10a,
	0xf10b,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf30e,	0xf702,	0xf30d,	0xf01c,	0xf701,	0xf205,	0xf114,	0xf603,
	0xf20b,	0xf601,	0xf602,	0xf117,	0xf600,	0xf20a,	0xf115,	0xf116,
	0xf11a,	0xf10c,	0xf10d,	0xf11b,	0xf11c,	0xf110,	0xf311,	0xf11d,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
};

int shift = 0;

char key_to_char(uint8_t key)
{
	return (shift ? shift_map[key] : plain_map[key]) & 0xFF;
}

void on_IRQ0()
{
	// hook on timer IRQ
	static int counter = 0;
	counter++;
	if(18 == counter)
	{
		counter = 0;
		if(gTerminal)
		{
			static int seconds = 0;
			seconds++;
			char temp[16];
			itoa(seconds, temp, 10);
			gTerminal->write("time ");
			gTerminal->write(temp);
			gTerminal->write("s\n");
		}
	}
}

void on_IRQ1()
{
	// hook for keyboard handler
	#define	KBDATAP		0x60	/* kbd data port */
	#define	KBSTATUSPORT	0x61	/* kbd status */
	#define	KBSTATP		0x64	/* kbd status port */
	#define	KBINRDY		0x01
	#define	KBOUTRDY	0x02
	
    if((inb(KBSTATP) & KBINRDY) == 0)
		return;
	
	// read scan code
	uint8_t scancode = inb(0x60);
	uint8_t brk = scancode & 0x80;
	uint8_t key = scancode & 0x7f;
		
	// debug print scan code
	uint16_t *screen = (uint16_t*)0xB8000;
	char *digit = "0123456789ABCDEF";
	screen[160] = digit[(scancode >> 4) & 0xf] + (7<<8);
	screen[161] = digit[scancode & 0xf] + (7<<8);
	
	// shifts
	if((key == 0x2A) || (key == 0x36))
	{
		shift = brk ? 0 : 1;
		return;
	}
	
	// terminal print char if any
	if(gTerminal && !brk)
	{
		char txt[4];
		txt[0] = key_to_char(key);
		if(txt[0] == '\n' || (txt[0] >= ' ' && txt[0] < 0x7f))
		{
			txt[1] = 0;
			gTerminal->write(txt);
		}
	}
}

void set_timer(uint16_t ticks)
{
	asm volatile ("cli");
	outb(0x43, 0x34); 
	outb(0x40, ticks & 0xFF);
	outb(0x40, (ticks >> 8) & 0xFF);
	asm volatile ("sti");	
}

void kernel_main(system_info *pinfo)
{
	system_info info = *pinfo;
	Terminal term((uint16_t*)0xB8000, 25, 80);
	gTerminal = &term;
	term.move(info.cursory, info.cursorx);
	term.write("[elf_kernel64] hello 2++!\n");
	term.write("[elf_kernel64] setting up interrupts\n");
	idt_init();
	set_irq_hook(0, on_IRQ0);
	set_irq_hook(1, on_IRQ1);
	term.write("[elf_kernel64] still alive\n");
	
	// start a timer
	set_timer(0); // once a second
	
stop:
	goto stop;
}

