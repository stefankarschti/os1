#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sysinfo.h"
#include "terminal.h"
#include "interrupt.h"
#include "memory.h"
#include "../libc/stdlib.h"
#include "task.h"
#include "pageframe.h"
#include "keyboard.h"

// system
PageFrameContainer page_frames;
Keyboard keyboard;


// multitasking
uint64_t stack1[512] __attribute__ ((aligned (4096)));
uint64_t stack2[512] __attribute__ ((aligned (4096)));
uint64_t stack3[512] __attribute__ ((aligned (4096)));
void process1();
void process2();
void process3();

// terminals
const size_t numTerminals = 3;
uint16_t* buffer[numTerminals] = {(uint16_t*)0x10000, (uint16_t*)0x11000, (uint16_t*)0x12000};
Terminal terminal[numTerminals];
Terminal *active_terminal = nullptr;

uint16_t SetTimer(uint16_t frequency)
{
	uint32_t divisor = 1193180 / frequency;
	if(divisor > 65536)
	{
		divisor = 65536; // max out to 18 Hz
	}
	outb(0x43, 0x34);
	outb(0x40, divisor & 0xFF);
	outb(0x40, (divisor >> 8) & 0xFF);
	// return actual frequency
	return 1193180 / divisor;
}

void KernelKeyboardHook(uint16_t scancode)
{
	// switch terminal hotkey
	if(scancode >= 0x3B && scancode <= 0x3D)
	{
		int index = scancode - 0x3B;
		if(active_terminal)
			active_terminal->unlink();
		active_terminal = &terminal[index];
		active_terminal->link();
		keyboard.SetActiveTerminal(active_terminal);
	}
}

void kernel_main(system_info *pinfo)
{
	// initialize terminals
	for(size_t i = 0; i < numTerminals; ++i)
	{
		terminal[i].setBuffer(buffer[i]);
		terminal[i].clear();
	}

	// activate terminal 0
	active_terminal = &terminal[0];
	active_terminal->link();

	// greetings
	active_terminal->write("[elf_kernel64] hello\n");

	// VM
	active_terminal->write("[elf_kernel64] initializing page frame allocator\n");
	for(int i = 0; i < pinfo->num_memory_blocks; ++i)
	{
		char temp[32];
		memory_block &b = pinfo->memory_blocks[i];
		itoa(b.start, temp, 16, 16);	active_terminal->write(temp); active_terminal->write(" ");
		itoa(b.length, temp, 16, 16);	active_terminal->write(temp); active_terminal->write(" ");
		itoa(b.type, temp, 16);			active_terminal->write(temp); active_terminal->write("\n");
	}

	bool result = page_frames.Initialize(pinfo);
	if(result)
		active_terminal->write("Page frame initialization successful\n");
	else
		active_terminal->write("Page frame initialization failed\n");

	// print page frame debug info
	{
		char temp[32];
		uint64_t num;

		// memory size
		num = page_frames.MemorySize() >> 20; // MB
		itoa(num, temp, 10);
		active_terminal->write("Memory size ");
		active_terminal->write(temp);
		active_terminal->write(" MB\n");

		num = page_frames.MemoryEnd();
		itoa(num, temp, 16, 16);
		active_terminal->write("Memory end 0x");
		active_terminal->write(temp);
		active_terminal->write("\n");

		num = page_frames.PageCount();
		itoa(num, temp, 10);
		active_terminal->write("Page count ");
		active_terminal->write(temp);
		uint64_t num2 = (num + 7) / 8;
		itoa(num2, temp, 10);
		active_terminal->write(", bitmap size is ");
		active_terminal->write(temp);
		active_terminal->write(" bytes\n");
	}

	// set up interrupts
	active_terminal->write("[elf_kernel64] setting up interrupts\n");
	idt_init();

	// set up keyboard
	active_terminal->write("[elf_kernel64] starting keyboard\n");
	keyboard.Initialize();
	keyboard.SetActiveTerminal(active_terminal);


	// multitasking
	active_terminal->write("[elf_kernel64] initializing multitasking\n");
	initTasks();
	Task* task1 = newTask((void*)process1, stack1, 512);
	Task* task2 = newTask((void*)process2, stack2, 512);
	Task* task3 = newTask((void*)process3, stack3, 512);

	// start multitasking
	active_terminal->write("\n\n\nPress F1 F2 F3 to switch terminals\n\n\n");
	startMultiTask(task1);

	// we should not reach this point
	if(task2 == task3) //
		active_terminal->write("[kernel64] panic! multitasking ended; halting.\n");

stop:
	asm volatile("hlt");
	goto stop;
}

void process1()
{
	Terminal *myTerminal = &terminal[0];
	myTerminal->write("process1 starting\n");
	int x = 100;
	while(x--)
	{
		myTerminal->write("1");
	}
	myTerminal->write("\nprocess1 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process2()
{
	Terminal *myTerminal = &terminal[1];
	myTerminal->write("process2 starting\n");
	int x = 100;
	while(x--)
	{
		char line[256];
		line[0] = 0;
		myTerminal->write("2>");
		//myTerminal->readline(line);
		myTerminal->write("You said ");
		myTerminal->write(line);
		myTerminal->write("\n");
	}
	myTerminal->write("\nprocess2 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process3()
{
	Terminal *myTerminal = &terminal[2];
	myTerminal->write("process3 starting\n");
	int x = 100;
	while(x--)
	{
		myTerminal->write("3");
	}
	myTerminal->write("\nprocess3 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}
