#include "interrupt.h"
#include "memory.h"

struct IDTDescriptor IDT[256];

extern int load_idt();
extern int irq0();
extern int irq1();
extern int irq2();
extern int irq3();
extern int irq4();
extern int irq5();
extern int irq6();
extern int irq7();
extern int irq8();
extern int irq9();
extern int irq10();
extern int irq11();
extern int irq12();
extern int irq13();
extern int irq14();
extern int irq15();

extern int int_80h();

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

void idt_init(void)
{
	for(int i = 0; i < 256; i++)
	{
		clr_IDT(i);
		set_IDT(i, (uint64_t)int_80h);		
	}
/*
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
 
	set_IDT(32, (uint64_t)irq0);
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
*/

	set_IDT(0x80, int_80h);
	
	// fill the IDT descriptor
	struct IDT_PTR idt_ptr;
	idt_ptr.limit = sizeof (struct IDTDescriptor) * 256;
	idt_ptr.base = (uint64_t)IDT;

	// load IDT
	load_idt(idt_ptr);

}

void irq0_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq1_handler(void) {
	  outb(0x20, 0x20); //EOI
}
 
void irq2_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq3_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq4_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq5_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq6_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq7_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq8_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI          
}
 
void irq9_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq10_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq11_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq12_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq13_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq14_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}
 
void irq15_handler(void) {
          outb(0xA0, 0x20);
          outb(0x20, 0x20); //EOI
}

void int_handler(int number)
{
	// software interrupt
	if(0x80 == number)
	{
		// got it
		uint16_t *screen = (uint16_t*)0xB8000;
		screen[80] = '8' + (7<<8);
		screen[81] = '0' + (7<<8);
		screen[82] = 'h' + (7<<8);
	}
}
