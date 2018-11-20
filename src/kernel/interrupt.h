#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>
#include <stddef.h>

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

class Interrupts
{
public:
	bool Initialize();
	void SetIRQHandler(int number, void (*pFunction)(void*), void* data);
	void SetExceptionHandler(int number, void (*handler)(uint64_t, uint64_t, uint64_t));

private:
	IDTDescriptor IDT[256];
	void SetIDT(int index, uint64_t address);
	void ClearIDT(int index);
};

#endif 
