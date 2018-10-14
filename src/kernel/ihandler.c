#include <stdint.h>
#include "memory.h"

void (*(irq_hook[16]))();

void set_irq_hook(int number, void (*hook)())
{
	if(number >= 0 && number < 16)
	{
		irq_hook[number] = hook;
	}
}

void irq0_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[0]) irq_hook[0]();
}
 
void irq1_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[1]) irq_hook[1]();
}
 
void irq2_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[2]) irq_hook[2]();
}
 
void irq3_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[3]) irq_hook[3]();
}
 
void irq4_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[4]) irq_hook[4]();
}
 
void irq5_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[5]) irq_hook[5]();
}
 
void irq6_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[6]) irq_hook[6]();
}
 
void irq7_handler(void) {
	outb(0x20, 0x20); //EOI
	if(irq_hook[7]) irq_hook[7]();
}
 
void irq8_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI          
	if(irq_hook[8]) irq_hook[8]();
}
 
void irq9_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[9]) irq_hook[9]();
}
 
void irq10_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[10]) irq_hook[10]();
}
 
void irq11_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[11]) irq_hook[11]();
}
 
void irq12_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[12]) irq_hook[12]();
}
 
void irq13_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[13]) irq_hook[13]();
}
 
void irq14_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[14]) irq_hook[14]();
}
 
void irq15_handler(void) {
	outb(0xA0, 0x20);
	outb(0x20, 0x20); //EOI
	if(irq_hook[15]) irq_hook[15]();
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
	else
	{
		// got it
		uint16_t *screen = (uint16_t*)0xB8000;
		screen[80] = '?' + (7<<8);
		screen[81] = '?' + (7<<8);
		screen[82] = '?' + (7<<8);
	}
}
