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
const size_t kNumTerminals = 3;
uint16_t* buffer[kNumTerminals] = {(uint16_t*)0x10000, (uint16_t*)0x11000, (uint16_t*)0x12000};
Terminal terminal[kNumTerminals];
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

/**
 * @brief KernelKeyboardHook
 * @param scancode
 * @details this function is called from keyboard IRQ before any other keyboard handler
 * @ref keyboard.cpp
 * @result bool Whether the key should be further processed. Return false to silently ignore the key
 */
bool KernelKeyboardHook(uint16_t scancode)
{
	// switch terminal hotkey
	if(scancode >= 0x3B && scancode <= 0x3D)
	{
		int index = scancode - 0x3B;
		if(active_terminal)
			active_terminal->Unlink();
		active_terminal = &terminal[index];
		active_terminal->Link();
		keyboard.SetActiveTerminal(active_terminal);
	}
}

void kernel_main(system_info *info)
{
	// initialize terminals
	for(size_t i = 0; i < kNumTerminals; ++i)
	{
		terminal[i].SetBuffer(buffer[i]);
		terminal[i].Clear();
	}

	// activate terminal 0
	active_terminal = &terminal[0];
	active_terminal->Link();

	// greetings
	active_terminal->WriteLn("[elf_kernel64] hello");

	// VM
	active_terminal->WriteLn("[elf_kernel64] initializing page frame allocator");
	for(int i = 0; i < info->num_memory_blocks; ++i)
	{
		char temp[32];
		MemoryBlock &b = info->memory_blocks[i];
		itoa(b.start, temp, 16, 16);	active_terminal->Write(temp); active_terminal->Write(" ");
		itoa(b.length, temp, 16, 16);	active_terminal->Write(temp); active_terminal->Write(" ");
		itoa(b.type, temp, 16);			active_terminal->Write(temp); active_terminal->Write('\n');
	}

	bool result = page_frames.Initialize(info);
	if(result)
		active_terminal->WriteLn("Page frame initialization successful");
	else
		active_terminal->WriteLn("Page frame initialization failed");

	// print page frame debug info
	{
		char temp[32];
		uint64_t num;

		// memory size
		num = page_frames.MemorySize() >> 20; // MB
		itoa(num, temp, 10);
		active_terminal->Write("Memory size ");
		active_terminal->WriteLn(temp);

		num = page_frames.MemoryEnd();
		itoa(num, temp, 16, 16);
		active_terminal->Write("Memory end 0x");
		active_terminal->WriteLn(temp);

		num = page_frames.PageCount();
		itoa(num, temp, 10);
		active_terminal->Write("Page count ");
		active_terminal->Write(temp);
		uint64_t num2 = (num + 7) / 8;
		itoa(num2, temp, 10);
		active_terminal->Write(", bitmap size is ");
		active_terminal->Write(temp);
		active_terminal->WriteLn(" bytes\n");
	}

	// set up interrupts
	active_terminal->WriteLn("[elf_kernel64] setting up interrupts");
	idt_init();

	// set up keyboard
	active_terminal->WriteLn("[elf_kernel64] starting keyboard");
	keyboard.Initialize();
	keyboard.SetActiveTerminal(active_terminal);


	// multitasking
	active_terminal->WriteLn("[elf_kernel64] initializing multitasking");
	initTasks();
	Task* task1 = newTask((void*)process1, stack1, 512);
	Task* task2 = newTask((void*)process2, stack2, 512);
	Task* task3 = newTask((void*)process3, stack3, 512);

	// start multitasking
	active_terminal->Write("\n\n\nPress F1 F2 F3 to switch terminals\n\n\n");
	startMultiTask(task1);

	// we should not reach this point
	if(task2 == task3) //
		active_terminal->WriteLn("[kernel64] panic! multitasking ended; halting.");

stop:
	asm volatile("hlt");
	goto stop;
}

void process1()
{
	Terminal *myTerminal = &terminal[0];
	myTerminal->Write("process1 starting\n");
	int x = 100;
	while(x--)
	{
		myTerminal->Write("1");
	}
	myTerminal->Write("\nprocess1 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process2()
{
	Terminal *myTerminal = &terminal[1];
	myTerminal->Write("process2 starting\n");
	int x = 100;
	while(x--)
	{
		char line[256];
		line[0] = 0;
		myTerminal->Write("2>");
		//myTerminal->readline(line);
		myTerminal->Write("You said ");
		myTerminal->Write(line);
		myTerminal->Write("\n");
	}
	myTerminal->Write("\nprocess2 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process3()
{
	Terminal *myTerminal = &terminal[2];
	myTerminal->Write("process3 starting\n");
	int x = 100;
	while(x--)
	{
		myTerminal->Write("3");
	}
	myTerminal->Write("\nprocess3 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}
