#include <stdint.h>
#include "memory.h"

void (*(irq_hook[16]))(void*);
void *irq_data[16];

void set_irq_hook(int number, void (*hook)(void*), void *data)
{
	if(number >= 0 && number < 16)
	{
		irq_hook[number] = hook;
		irq_data[number] = data;
	}
}

// IRQ0: Timer 
void irq0_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[0]) irq_hook[0](irq_data[0]);
}
 
// IRQ1: Keyboard 
void irq1_handler(void) {
	if(irq_hook[1]) irq_hook[1](irq_data[1]);
}
 
// IRQ2: cascade IRQ8-15
void irq2_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[2]) irq_hook[2](irq_data[2]);
}
 
// IRQ3: COM2 / COM4
void irq3_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[3]) irq_hook[3](irq_data[3]);
}
 
// IRQ4: COM1 / COM3
void irq4_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[4]) irq_hook[4](irq_data[4]);
}
 
// IRQ5: LPT2 / LPT3 / Sound card
void irq5_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[5]) irq_hook[5](irq_data[5]);
}
 
// IRQ6: Floppy 
void irq6_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[6]) irq_hook[6](irq_data[6]);
}
 
// IRQ7: LPT1
void irq7_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[7]) irq_hook[7](irq_data[7]);
}
 
// IRQ8: Real Time Clock
void irq8_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI          
	if(irq_hook[8]) irq_hook[8](irq_data[8]);
}
 
// IRQ9: ACPI / free
void irq9_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[9]) irq_hook[9](irq_data[9]);
}
 
// IRQ10: free
void irq10_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[10]) irq_hook[10](irq_data[10]);
}
 
// IRQ11: free
void irq11_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[11]) irq_hook[11](irq_data[11]);
}
 
// IRQ12: PS2 mouse
void irq12_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[12]) irq_hook[12](irq_data[12]);
}
 
// IRQ13: IPI / Coprocessor
void irq13_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[13]) irq_hook[13](irq_data[13]);
}
 
// IRQ14: ATA1
void irq14_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[14]) irq_hook[14](irq_data[14]);
}
 
// IRQ15: ATA2
void irq15_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[15]) irq_hook[15](irq_data[15]);
}


///
/// CPU Exception Handlers
///
void (*(exception_handler_function[32]))(uint64_t, uint64_t, uint64_t);
void set_exception_handler(int number, void (*handler)(uint64_t, uint64_t, uint64_t))
{
	// set hook for exception interrupts
	if(number >= 0 && number < 32)
	{
		exception_handler_function[number] = handler;
	}
}

void exception_handler(uint64_t number, uint64_t rip, uint64_t rsp, uint64_t error)
{
	if(number < 32)
	{
		if(exception_handler_function[number]) exception_handler_function[number](rip, rsp, error);
	}
}
