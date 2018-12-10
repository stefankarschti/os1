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

///
/// IRQ Handler
///
void irq_handler(uint64_t number)
{
	if(number < 16 && irq_hook[number])
	{
		irq_hook[number](irq_data[number]);
	}
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
