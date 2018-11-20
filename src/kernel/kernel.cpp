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
void process1();
void process2();
void process3();

// terminals
const size_t kNumTerminals = 12;
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

void onException00(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#DE-Divide-by-Zero-Error Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException01(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#DB-Debug Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException02(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "NMI-Non-Maskable-Interrupt Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException03(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#BP-Breakpoint Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException04(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#OF-Overflow Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException05(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#BR-Bound-Range Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException06(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#UD-Invalid-Opcode Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException07(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#NM-Device-Not-Available Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException08(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#DF-Double-Fault Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException09(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "Coprocessor-Segment-Overrun Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException0A(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#TS-Invalid-TSS Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException0B(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#NP-Segment-Not-Present Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException0C(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#SS-Stack Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException0D(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );	

	const char *name = "#GP-General-Protection Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException0E(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#PF-Page-Fault Exception";
	if(active_terminal)
	{
		active_terminal->Write("#PF-Page-Fault Exception");
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException10(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#MF-x87 Floating-Point Exception-Pending";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException11(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#AC-Alignment-Check Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException12(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#MC-Machine-Check Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException13(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#XF-SIMD Floating-Point Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException1D(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#VC -- VMM Communication Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void onException1E(uint64_t rip, uint64_t rsp, uint64_t error)
{
	uint64_t cr2, cr3, r15;
	asm volatile( "mov %%cr2, %0" : "=r"(cr2) );
	asm volatile( "mov %%cr3, %0" : "=r"(cr3) );
	asm volatile( "mov %%r15, %0" : "=r"(r15) );

	const char *name = "#SX-Security Exception";
	if(active_terminal)
	{
		active_terminal->WriteLn(name);
	}
	debug(name)(" RIP=")(rip, 16)(" RSP=")(rsp, 16)(" error=")(error, 16)(" cr2=")(cr2, 16)(" cr3=")(cr3, 16)(" r15=")(r15, 16)();
	Registers *regs = (Registers*)(0x10000);
	regs->print();
	stop:
	asm("cli");
	asm("hlt");
	goto stop;
}

void KernelMain(SystemInformation *info)
{
	bool result;
	debug((uint64_t)(&debug), 16);
	debug("[kernel64] hello!\n");

	// deep copy system information
	SystemInformation sysinfo = *info;
	MemoryBlock memory_blocks[sysinfo.num_memory_blocks];
	memcpy(memory_blocks, info->memory_blocks, sizeof(MemoryBlock) * sysinfo.num_memory_blocks);
	sysinfo.memory_blocks = memory_blocks;

	// initialize page frames
	debug("initializing page frame allocator")();
	result = page_frames.Initialize(sysinfo, 0x20000, 0x40000 / 8);
	debug(result ? "Success" : "Failure")();
	if(!result) return;

	// create kernel page tables
	VirtualMemory kvm(page_frames);
	debug("create kernel identity page tables")();
	result = kvm.Allocate(0x0, 16 * 256, true);
	debug(result ? "Success" : "Failure")();
	if(!result) return;

	// switch to kernel page frames
	kvm.Activate();
	debug("kvm activated")();

	// initialize terminals
	for(size_t i = 0; i < kNumTerminals; ++i)
	{
		uint64_t p;
		if(page_frames.Allocate(p))
		{
			debug("allocate terminal ")(i + 1)(" at 0x")(p, 16)();
			terminal[i].SetBuffer((uint16_t*)p);
			terminal[i].Clear();
			terminal[i].Write("Terminal ");
			terminal[i].WriteIntLn(i + 1);
		}
	}

	// activate terminal 0
	active_terminal = &terminal[0];
	active_terminal->Copy((uint16_t*)0xB8000);
	active_terminal->Link();
	active_terminal->MoveCursor(sysinfo.cursory, sysinfo.cursorx);

	// greetings
	active_terminal->WriteLn("[kernel64] hello");

	// print page frame debug info
	{
		char temp[32];
		uint64_t num;

		// memory size
		num = page_frames.MemorySize() >> 20; // MB
		utoa(num, temp, 10);
		active_terminal->Write("Memory size ");
		active_terminal->Write(temp);
		active_terminal->WriteLn("MB");

		num = page_frames.MemoryEnd();
		utoa(num, temp, 16, 16);
		active_terminal->Write("Memory end 0x");
		active_terminal->WriteLn(temp);

		num = page_frames.PageCount();
		utoa(num, temp, 10);
		active_terminal->Write(temp);
		active_terminal->WriteLn(" total pages");

		num = page_frames.FreePageCount();
		utoa(num, temp, 10);
		active_terminal->Write(temp);
		active_terminal->WriteLn(" free pages");

		uint64_t num2 = (page_frames.PageCount() + 7) / 8;
		utoa(num2, temp, 10);
		active_terminal->Write("Bitmap size is ");
		active_terminal->Write(temp);
		active_terminal->WriteLn(" bytes");
	}

	// set up interrupts
	active_terminal->WriteLn("[kernel64] setting up interrupts");
	result = interrupts.Initialize();
	if(result)
		active_terminal->WriteLn("Interrupts initialization successful");
	else
		active_terminal->WriteLn("Interrupts initialization failed");
	active_terminal->WriteLn("Set up #PF");

	// set exception handlers
	interrupts.SetExceptionHandler( 0, onException00);
	interrupts.SetExceptionHandler( 1, onException01);
	interrupts.SetExceptionHandler( 2, onException02);
	interrupts.SetExceptionHandler( 3, onException03);
	interrupts.SetExceptionHandler( 4, onException04);
	interrupts.SetExceptionHandler( 5, onException05);
	interrupts.SetExceptionHandler( 6, onException06);
	interrupts.SetExceptionHandler( 7, onException07);
	interrupts.SetExceptionHandler( 8, onException08);
	interrupts.SetExceptionHandler( 9, onException09);
	interrupts.SetExceptionHandler(10, onException0A);
	interrupts.SetExceptionHandler(11, onException0B);
	interrupts.SetExceptionHandler(12, onException0C);
	interrupts.SetExceptionHandler(13, onException0D);
	interrupts.SetExceptionHandler(14, onException0E);
	interrupts.SetExceptionHandler(16, onException10);
	interrupts.SetExceptionHandler(17, onException11);
	interrupts.SetExceptionHandler(18, onException12);
	interrupts.SetExceptionHandler(19, onException13);
	interrupts.SetExceptionHandler(29, onException1D);
	interrupts.SetExceptionHandler(30, onException1E);

//	active_terminal->WriteLn("Trigger #PF");
//	uint64_t *p = (uint64_t*)0x0000DDFD200008;
//	uint64_t a = *p;
//	*p = a + 1;

	// set up keyboard
	active_terminal->WriteLn("[kernel64] starting keyboard");
	keyboard.Initialize();
	keyboard.SetActiveTerminal(active_terminal);

	// multitasking
	asm volatile("cli");
	active_terminal->WriteLn("[kernel64] initializing multitasking");
	debug("[kernel64] initializing multitasking")();
	uint64_t stack1;
	uint64_t stack2;
	uint64_t stack3;
	const uint64_t k_stack_num_pages = 1;
	result = page_frames.Allocate(stack1, k_stack_num_pages);
	if(result) 	debug("alloc stack1 at 0x")(stack1, 16)(); else debug("alloc stack1 failed")();
	if(!result) return;
	result = page_frames.Allocate(stack2, k_stack_num_pages);
	if(result) 	debug("alloc stack2 at 0x")(stack2, 16)(); else debug("alloc stack2 failed")();
	if(!result) return;
	result = page_frames.Allocate(stack3, k_stack_num_pages);
	if(result) 	debug("alloc stack3 at 0x")(stack3, 16)(); else debug("alloc stack3 failed")();
	if(!result) return;

	initTasks();
	debug("sizeof Task is ")(sizeof(Task))();
	Task* task1 = newTask((void*)process1, (uint64_t*)stack1, k_stack_num_pages * 4096 / 8);
	Task* task2 = newTask((void*)process2, (uint64_t*)stack2, k_stack_num_pages * 4096 / 8);
//	Task* task3 = newTask((void*)process3, (uint64_t*)stack3, k_stack_num_pages * 4096 / 8);

	// start multitasking
	if(task1 /*&& task2 && task3*/)
	{
		debug("start multitasking")();
		active_terminal->WriteLn("Press F1..F12 to switch terminals");

		debug("task1 pid ")(task1->pid)();
		task1->regs.print();
		debug("interrupt stack:")();
		debug("RIP=")(*(uint64_t*)(task1->regs.rsp), 16)();
		debug("CS=")(*(uint64_t*)(task1->regs.rsp + 8), 16)();
		debug("RFLAGS=")(*(uint64_t*)(task1->regs.rsp + 16), 16)();
		debug("RSP=")(*(uint64_t*)(task1->regs.rsp + 24), 16)();
		debug("SS=")(*(uint64_t*)(task1->regs.rsp + 32), 16)();

		startMultiTask(task1);
	}
	else
	{
		debug("Task creation failed")();
		active_terminal->WriteLn("Task creation failed");
	}

	// we should not reach this point
	active_terminal->WriteLn("[kernel64] panic! multitasking ended; halting.");

stop:
	asm volatile("hlt");
	goto stop;
}

void process1()
{
	Terminal *my_terminal = &terminal[0];
	while(true)
	{
		my_terminal->Write('1');
	}
//	Terminal *my_terminal = &terminal[0];
//	my_terminal->Write("process1 starting\n");
//	while(true)
//	{
//		extern Task *taskList;
//		Task* task = &taskList[0];
//		my_terminal->Write("task 0x");
//		my_terminal->WriteIntLn((uint64_t)task, 16);
//		my_terminal->Write("pid=");
//		my_terminal->WriteIntLn(task->pid);
//		my_terminal->Write("cr3=");
//		my_terminal->WriteIntLn(task->regs.cr3, 16);
//		my_terminal->Write("rsp=");
//		my_terminal->WriteIntLn(task->regs.rsp, 16);

//		char line[256];
//		line[0] = 0;
//		my_terminal->Write("terminal1:");
//		my_terminal->ReadLn(line);
//		my_terminal->Write("Command: ");
//		my_terminal->WriteLn(line);
//	}
//	my_terminal->Write("\nprocess1 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process2()
{
	Terminal *my_terminal = &terminal[1];
	while(true)
	{
		my_terminal->Write('2');
	}
//	Terminal *my_terminal = &terminal[1];
//	my_terminal->Write("process2 starting\n");
//	while(true)
//	{
//		extern Task *taskList;
//		Task* task = &taskList[1];
//		my_terminal->Write("task 0x");
//		my_terminal->WriteIntLn((uint64_t)task, 16);
//		my_terminal->Write("pid=");
//		my_terminal->WriteIntLn(task->pid);
//		my_terminal->Write("cr3=");
//		my_terminal->WriteIntLn(task->regs.cr3, 16);
//		my_terminal->Write("rsp=");
//		my_terminal->WriteIntLn(task->regs.rsp, 16);

//		char line[256];
//		line[0] = 0;
//		my_terminal->Write("terminal2:");
//		my_terminal->ReadLn(line);
//		my_terminal->Write("Command: ");
//		my_terminal->WriteLn(line);
//	}
//	my_terminal->Write("\nprocess2 ending\n");
stop:
	asm volatile("hlt");
	goto stop;
}

void process3()
{
	debug("process3 starting")();
	Terminal *my_terminal = &terminal[2];
	my_terminal->WriteLn("process3 starting");
//	while(true)
//	{
//		extern Task *taskList;
//		Task* task = &taskList[2];
//		debug("task3")();
//		task->regs.print();
//	}
	while(true)
	{
		extern Task *taskList;
		Task* task = &taskList[2];
		debug("task3")();
		task->regs.print();
		my_terminal->Write("task 0x");
		my_terminal->WriteIntLn((uint64_t)task, 16);
		my_terminal->Write("pid=");
		my_terminal->WriteIntLn(task->pid);
		my_terminal->Write("cr3=");
		my_terminal->WriteIntLn(task->regs.cr3, 16);
		my_terminal->Write("rsp=");
		my_terminal->WriteIntLn(task->regs.rsp, 16);

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
