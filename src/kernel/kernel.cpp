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
#include "virtualmemory.h"
#include "keyboard.h"
#include "debug.h"

// system
Interrupts interrupts;
PageFrameContainer page_frames;
Keyboard keyboard(interrupts);

// multitasking
uint64_t stack1[512] __attribute__ ((aligned (4096)));
uint64_t stack2[512] __attribute__ ((aligned (4096)));
uint64_t stack3[512] __attribute__ ((aligned (4096)));
void process1();
void process2();
void process3();

// terminals
const size_t kNumTerminals = 12;
uint16_t* terminal_buffer[kNumTerminals] =
{
	(uint16_t*)0x10000,
	(uint16_t*)0x11000,
	(uint16_t*)0x12000,
	(uint16_t*)0x13000,
	(uint16_t*)0x14000,
	(uint16_t*)0x15000,
	(uint16_t*)0x16000,
	(uint16_t*)0x17000,
	(uint16_t*)0x18000,
	(uint16_t*)0x19000,
	(uint16_t*)0x1A000,
	(uint16_t*)0x1B000,
};
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
	uint16_t hotkey[kNumTerminals] =  {0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58};
	int index = -1;
	for(unsigned i = 0; i < kNumTerminals; i++)
	{
		if(scancode == hotkey[i])
		{
			index = i;
			break;
		}
	}
	if(index >= 0)
	{
		if(active_terminal != &terminal[index])
		{
			if(active_terminal)
				active_terminal->Unlink();
			active_terminal = &terminal[index];
			active_terminal->Link();
			keyboard.SetActiveTerminal(active_terminal);
		}
	}

	// debug print scan code
//	uint16_t *screen = (uint16_t*)0xB8000;
//	const char *digit = "0123456789ABCDEF";
//	screen[160] = digit[(scancode >> 4) & 0xf] + (7<<8);
//	screen[161] = digit[scancode & 0xf] + (7<<8);
	return true;
}

void KernelMain(SystemInformation *info)
{
	debug.Write("[kernel64] hello!\n");

	// deep copy system information
	SystemInformation sysinfo = *info;
	MemoryBlock memory_blocks[sysinfo.num_memory_blocks];
	memcpy(memory_blocks, info->memory_blocks, sizeof(MemoryBlock) * sysinfo.num_memory_blocks);
	sysinfo.memory_blocks = memory_blocks;

	// initialize terminals
	for(size_t i = 0; i < kNumTerminals; ++i)
	{
		terminal[i].SetBuffer(terminal_buffer[i]);
		terminal[i].Clear();
		terminal[i].Write("Terminal ");
		terminal[i].WriteIntLn(i + 1);
	}

	// activate terminal 0
	active_terminal = &terminal[0];
	active_terminal->Copy((uint16_t*)0xB8000);
	active_terminal->Link();
	active_terminal->MoveCursor(sysinfo.cursory, sysinfo.cursorx);

	// greetings
	active_terminal->WriteLn("[kernel64] hello");

	// initialize page frames
	active_terminal->WriteLn("[kernel64] initializing page frame allocator");
	bool result = page_frames.Initialize(sysinfo);
	active_terminal->WriteLn(result ? "Success" : "Failure");

	// print page frame debug info
	{
		char temp[32];
		uint64_t num;

		// memory size
		num = page_frames.MemorySize() >> 20; // MB
		itoa(num, temp, 10);
		active_terminal->Write("Memory size ");
		active_terminal->Write(temp);
		active_terminal->WriteLn("MB");

		num = page_frames.MemoryEnd();
		itoa(num, temp, 16, 16);
		active_terminal->Write("Memory end 0x");
		active_terminal->WriteLn(temp);

		num = page_frames.PageCount();
		itoa(num, temp, 10);
		active_terminal->Write(temp);
		active_terminal->WriteLn(" total pages");

		num = page_frames.FreePageCount();
		itoa(num, temp, 10);
		active_terminal->Write(temp);
		active_terminal->WriteLn(" free pages");

		uint64_t num2 = (page_frames.PageCount() + 7) / 8;
		itoa(num2, temp, 10);
		active_terminal->Write("Bitmap size is ");
		active_terminal->Write(temp);
		active_terminal->WriteLn(" bytes");

		if((false))
		{
			// test allocation
			uint64_t address = 0xFFFFFFFFFFFFFFFF;
			for(int i = 0; i < 10; i++)
			{
				result = page_frames.Allocate(address);
				if(result)
				{
					active_terminal->Write("page_frame.Allocate returned ");
					active_terminal->WriteIntLn(address, 16, 16);
				}
				else
				{
					active_terminal->WriteLn("page_frame.Allocate failed");
				}
			}
			// test deallocation
			for(int i = 0; i < 10; i++)
			{
				address = 0x1000 * i;
				result = page_frames.Free(address);
				if(result)
				{
					active_terminal->WriteLn("page_frame.Free success");
				}
				else
				{
					active_terminal->WriteLn("page_frame.Free failed");
				}
			}
		}
	}

	// set up VM for kernel
	VirtualMemory vm(page_frames);

	active_terminal->WriteLn("Allocating some virtual memory");
	debug(vm.Allocate(0x1000, 3) ? "Success" : "Failure")();
//	debug(vm.Free(0x0, 3) ? "Success" : "Failure")();
//	debug(vm.Allocate(0x100000, 3) ? "Success" : "Failure")();
//	debug(vm.Allocate(0x8000000000, 5) ? "Success" : "Failure")();
//	debug(vm.Allocate(0x8000000000, 2) ? "Success" : "Failure")();
//	debug(vm.Allocate(0x1000, 262144) ? "Success" : "Failure")();
	debug(vm.Free() ? "Success" : "Failure")();

	// set up interrupts
	active_terminal->WriteLn("[kernel64] setting up interrupts");
	result = interrupts.Initialize();
	if(result)
		active_terminal->WriteLn("Interrupts initialization successful");
	else
		active_terminal->WriteLn("Interrupts initialization failed");

	// set up keyboard
	active_terminal->WriteLn("[kernel64] starting keyboard");
	keyboard.Initialize();
	keyboard.SetActiveTerminal(active_terminal);


	// multitasking
	active_terminal->WriteLn("[elf_kernel64] initializing multitasking");
	initTasks();
	Task* task1 = newTask((void*)process1, stack1, 512);
	Task* task2 = newTask((void*)process2, stack2, 512);
	Task* task3 = newTask((void*)process3, stack3, 512);

	// start multitasking
	active_terminal->WriteLn("Press F1..F12 to switch terminals");
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
	Terminal *my_terminal = &terminal[0];
	my_terminal->Write("process1 starting\n");
	while(true)
	{
		char line[256];
		line[0] = 0;
		my_terminal->Write("terminal1:");
		my_terminal->ReadLn(line);
		my_terminal->Write("Command: ");
		my_terminal->WriteLn(line);
	}
	my_terminal->Write("\nprocess1 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process2()
{
	Terminal *my_terminal = &terminal[1];
	my_terminal->Write("process2 starting\n");
	while(true)
	{
		char line[256];
		line[0] = 0;
		my_terminal->Write("terminal2:");
		my_terminal->ReadLn(line);
		my_terminal->Write("Command: ");
		my_terminal->WriteLn(line);
	}
	my_terminal->Write("\nprocess2 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process3()
{
	Terminal *my_terminal = &terminal[2];
	my_terminal->Write("process3 starting\n");
	while(true)
	{
		char line[256];
		line[0] = 0;
		my_terminal->Write("terminal3:");
		my_terminal->ReadLn(line);
		my_terminal->Write("Command: ");
		my_terminal->WriteLn(line);
	}
	my_terminal->Write("\nprocess3 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}
