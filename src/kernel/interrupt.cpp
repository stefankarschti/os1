#include "interrupt.h"
#include "memory.h"


extern "C" {
int task_switch_irq();

int irq0();
int irq1();
int irq2();
int irq3();
int irq4();
int irq5();
int irq6();
int irq7();
int irq8();
int irq9();
int irq10();
int irq11();
int irq12();
int irq13();
int irq14();
int irq15();

int int_00h();	// #DE
int int_01h();	// #DB
int int_02h();	// NMI
int int_03h();	// #BP
int int_04h();	// #OF
int int_05h();	// #BR
int int_06h();	// #UD
int int_07h();	// #NM
int int_08h();	// #DF
int int_09h();	// coprocessor
int int_0Ah();	// #TS
int int_0Bh();	// #NP
int int_0Ch();	// #SS
int int_0Dh();	// #GP
int int_0Eh();	// #PF page fault

int int_10h();	// #MF
int int_11h();	// #AC
int int_12h();	// #MC
int int_13h();	// #XF

int int_1Dh();	// #VC
int int_1Eh();	// #SX
}

extern "C" void set_irq_hook(int number, void (*pFunction)(void*), void* data);
extern "C" void set_exception_handler(int number, void (*handler)(uint64_t, uint64_t, uint64_t));

void Interrupts::SetIDT(int index, uint64_t address)
{
	IDT[index].offset_1 = address & 0xffff;
	IDT[index].selector = 0x08; /* KERNEL_CODE_SEGMENT_OFFSET */
	IDT[index].ist = 0;
	IDT[index].type_attr = 0x8e; /* INTERRUPT_GATE */
	IDT[index].offset_2 = (address >> 16) & 0xffff;
	IDT[index].offset_3 = (address >> 32) & 0xffffffff;
	IDT[index].zero = 0;
}

void Interrupts::ClearIDT(int index)
{
	IDT[index].offset_1 = 0;
	IDT[index].selector = 0;
	IDT[index].ist = 0;
	IDT[index].type_attr = 0;
	IDT[index].offset_2 = 0;
	IDT[index].offset_3 = 0;
	IDT[index].zero = 0;
}

static inline void lidt(void* base, uint16_t size)
{   // This function works in 32 and 64bit mode
	struct {
		uint16_t length;
		void*    base;
	} __attribute__((packed)) IDTR = { size, base };

	asm volatile ( "lidt %0" : : "m"(IDTR) );  // let the compiler choose an addressing mode
}

bool Interrupts::Initialize()
{
	asm volatile ("cli");
	for(int i = 0; i < 256; i++)
	{
		ClearIDT(i);
	}

	// remapping the PIC
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 40);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x0);
	outb(0xA1, 0x0);

//	SetIDT(32, (uint64_t)task_switch_irq);
	SetIDT(32, (uint64_t)irq0);
	SetIDT(33, (uint64_t)irq1);
	SetIDT(34, (uint64_t)irq2);
	SetIDT(35, (uint64_t)irq3);
	SetIDT(36, (uint64_t)irq4);
	SetIDT(37, (uint64_t)irq5);
	SetIDT(38, (uint64_t)irq6);
	SetIDT(39, (uint64_t)irq7);
	SetIDT(40, (uint64_t)irq8);
	SetIDT(41, (uint64_t)irq9);
	SetIDT(42, (uint64_t)irq10);
	SetIDT(43, (uint64_t)irq11);
	SetIDT(44, (uint64_t)irq12);
	SetIDT(45, (uint64_t)irq13);
	SetIDT(46, (uint64_t)irq14);
	SetIDT(47, (uint64_t)irq15);

	// set exception vectors
	SetIDT(0x00, (uint64_t)int_00h);
	SetIDT(0x01, (uint64_t)int_01h);
	SetIDT(0x02, (uint64_t)int_02h);
	SetIDT(0x03, (uint64_t)int_03h);
	SetIDT(0x04, (uint64_t)int_04h);
	SetIDT(0x05, (uint64_t)int_05h);
	SetIDT(0x06, (uint64_t)int_06h);
	SetIDT(0x07, (uint64_t)int_07h);
	SetIDT(0x08, (uint64_t)int_08h);
	SetIDT(0x09, (uint64_t)int_09h);
	SetIDT(0x0A, (uint64_t)int_0Ah);
	SetIDT(0x0B, (uint64_t)int_0Bh);
	SetIDT(0x0C, (uint64_t)int_0Ch);
	SetIDT(0x0D, (uint64_t)int_0Dh);
	SetIDT(0x0E, (uint64_t)int_0Eh);
	SetIDT(0x10, (uint64_t)int_10h);
	SetIDT(0x11, (uint64_t)int_11h);
	SetIDT(0x12, (uint64_t)int_12h);
	SetIDT(0x13, (uint64_t)int_13h);
	SetIDT(0x1D, (uint64_t)int_1Dh);
	SetIDT(0x1E, (uint64_t)int_1Eh);

	// clear irq hooks
	for(int i = 0; i < 16; ++i)
	{
		set_irq_hook(i, nullptr, nullptr);
	}

	// clear int hooks
	for(int i = 0; i < 256; ++i)
	{
		set_exception_handler(i, nullptr);
	}

	// load IDT
	lidt(IDT, 256 * sizeof(IDTDescriptor));
	asm volatile ("sti");
	return true;
}

void Interrupts::SetIRQHandler(int number, void (*pFunction)(void *), void *data)
{
	set_irq_hook(number, pFunction, data);
}

void Interrupts::SetExceptionHandler(int number, void (*handler)(uint64_t, uint64_t, uint64_t))
{
	set_exception_handler(number, handler);
}
