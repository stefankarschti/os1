#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>

struct IDTDescriptor
{
   uint16_t offset_1; // offset bits 0..15
   uint16_t selector; // a code segment selector in GDT or LDT
   uint8_t ist;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
   uint8_t type_attr; // type and attributes
   uint16_t offset_2; // offset bits 16..31
   uint32_t offset_3; // offset bits 32..63
   uint32_t zero;     // reserved
} __attribute__((packed));

void idt_init();

extern "C" void set_irq_hook(int number, void (*pFunction)());

#endif 
