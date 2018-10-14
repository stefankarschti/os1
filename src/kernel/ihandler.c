#include <stdint.h>
#include "memory.h"

void irq0_handler(void) {
          outb(0x20, 0x20); //EOI
}
 
void irq1_handler(void) // Keyboard IRQ
{
	outb(0x20, 0x20); //EOI
	uint8_t scancode = inb(0x60);
	uint16_t *screen = (uint16_t*)0xB8000;
	char *digit = "0123456789ABCDEF";
	screen[160] = digit[(scancode >> 4) & 0xf] + (7<<8);
	screen[161] = digit[scancode & 0xf] + (7<<8);
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
	else
	{
		// got it
		uint16_t *screen = (uint16_t*)0xB8000;
		screen[80] = '?' + (7<<8);
		screen[81] = '?' + (7<<8);
		screen[82] = '?' + (7<<8);
	}
}
