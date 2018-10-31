#include "interrupt.h"
#include "memory.h"

IDTDescriptor IDT[256];

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

	int int_80h();
}

void set_IDT(int index, uint64_t address)
{
	IDT[index].offset_1 = address & 0xffff;
	IDT[index].selector = 0x08; /* KERNEL_CODE_SEGMENT_OFFSET */
	IDT[index].ist = 0;
	IDT[index].type_attr = 0x8e; /* INTERRUPT_GATE */
	IDT[index].offset_2 = (address >> 16) & 0xffff;
	IDT[index].offset_3 = (address >> 32) & 0xffffffff;
	IDT[index].zero = 0;
}

void clr_IDT(int index)
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

void idt_init()
{
    asm volatile ("cli");
    for(int i = 0; i < 256; i++)
    {
        clr_IDT(i);
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

    set_IDT(32, (uint64_t)task_switch_irq);
    set_IDT(33, (uint64_t)irq1);
    set_IDT(34, (uint64_t)irq2);
    set_IDT(35, (uint64_t)irq3);
    set_IDT(36, (uint64_t)irq4);
    set_IDT(37, (uint64_t)irq5);
    set_IDT(38, (uint64_t)irq6);
    set_IDT(39, (uint64_t)irq7);
    set_IDT(40, (uint64_t)irq8);
    set_IDT(41, (uint64_t)irq9);
    set_IDT(42, (uint64_t)irq10);
    set_IDT(43, (uint64_t)irq11);
    set_IDT(44, (uint64_t)irq12);
    set_IDT(45, (uint64_t)irq13);
    set_IDT(46, (uint64_t)irq14);
    set_IDT(47, (uint64_t)irq15);

    set_IDT(0x80, (uint64_t)int_80h);

    // clear irq hooks
    for(int i = 0; i < 16; ++i)
    {
        set_irq_hook(i, nullptr);
    }

    // load IDT
    lidt(IDT, 256 * sizeof(IDTDescriptor));
    asm volatile ("int $0x80");
    asm volatile ("sti");
}


