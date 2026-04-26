#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>
#include <stddef.h>

#include "trapframe.h"

// Trap numbers
#define T_DIVIDE	0
#define T_DEBUG		1
#define T_NMI		2
#define T_BRKPT		3
#define T_OFLOW		4
#define T_BOUND		5
#define T_ILLOP		6
#define T_DEVICE	7
#define T_DBLFLT	8
#define T_TSS		10
#define T_SEGNP		11
#define T_STACK		12
#define T_GPFLT		13
#define T_PGFLT		14
#define T_FPERR		16
#define T_ALIGN		17
#define T_MCHK		18
#define T_SIMD		19
#define T_SECEV		30

#define T_IRQ0		32
#define T_SYSCALL	0x80
#define T_LTIMER	49
#define T_LERROR	50

#define IRQ_TIMER	0
#define IRQ_KBD		1
#define IRQ_SERIAL	4
#define IRQ_SPURIOUS	7
#define IRQ_IDE		14

struct IDTDescriptor
{
   uint16_t offset_1;
   uint16_t selector;
   uint8_t ist;
   uint8_t type_attr;
   uint16_t offset_2;
   uint32_t offset_3;
   uint32_t zero;
} __attribute__((packed));

using ExceptionHandler = void (*)(TrapFrame *);

class Interrupts
{
public:
	bool Initialize();
	void SetIRQHandler(int number, void (*pFunction)(void*), void* data);
	void SetExceptionHandler(int number, ExceptionHandler handler);

private:
	IDTDescriptor IDT[256];
	void SetIDT(int index, uint64_t address, uint8_t type_attr = 0x8E);
	void ClearIDT(int index);
};

void DispatchIRQHook(int number);
void DispatchExceptionHandler(int number, TrapFrame *frame);

#endif
