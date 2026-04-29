#include "arch/x86_64/interrupt/interrupt.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/io_port.hpp"
#include "util/memory.h"

extern "C"
{
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

    int int_00h();
    int int_01h();
    int int_02h();
    int int_03h();
    int int_04h();
    int int_05h();
    int int_06h();
    int int_07h();
    int int_08h();
    int int_09h();
    int int_0Ah();
    int int_0Bh();
    int int_0Ch();
    int int_0Dh();
    int int_0Eh();
    int int_10h();
    int int_11h();
    int int_12h();
    int int_13h();
    int int_1Dh();
    int int_1Eh();
}

namespace
{
void (*irq_hook[16])(void*) = {};
void* irq_data[16] = {};
ExceptionHandler exception_handler_function[256] = {};

static inline void lidt(void* base, uint16_t size)
{
    struct
    {
        uint16_t length;
        void* base;
    } __attribute__((packed)) idtr = {size, base};

    asm volatile("lidt %0" : : "m"(idtr));
}
}  // namespace

void dispatch_irq_hook(int number)
{
    if((number >= 0) && (number < 16) && irq_hook[number])
    {
        irq_hook[number](irq_data[number]);
    }
}

void dispatch_exception_handler(int number, TrapFrame* frame)
{
    if((number >= 0) && (number < 256) && exception_handler_function[number])
    {
        exception_handler_function[number](frame);
    }
}

void Interrupts::set_idt(int index, uint64_t address, uint8_t type_attr)
{
    IDT[index].offset_1 = address & 0xffff;
    IDT[index].selector = CPU_GDT_KCODE;
    IDT[index].ist = 0;
    IDT[index].type_attr = type_attr;
    IDT[index].offset_2 = (address >> 16) & 0xffff;
    IDT[index].offset_3 = (address >> 32) & 0xffffffff;
    IDT[index].zero = 0;
}

void Interrupts::clear_idt(int index)
{
    IDT[index].offset_1 = 0;
    IDT[index].selector = 0;
    IDT[index].ist = 0;
    IDT[index].type_attr = 0;
    IDT[index].offset_2 = 0;
    IDT[index].offset_3 = 0;
    IDT[index].zero = 0;
}

bool Interrupts::initialize()
{
    asm volatile("cli");
    for(int i = 0; i < 256; i++)
    {
        clear_idt(i);
    }

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

    set_idt(32, (uint64_t)irq0);
    set_idt(33, (uint64_t)irq1);
    set_idt(34, (uint64_t)irq2);
    set_idt(35, (uint64_t)irq3);
    set_idt(36, (uint64_t)irq4);
    set_idt(37, (uint64_t)irq5);
    set_idt(38, (uint64_t)irq6);
    set_idt(39, (uint64_t)irq7);
    set_idt(40, (uint64_t)irq8);
    set_idt(41, (uint64_t)irq9);
    set_idt(42, (uint64_t)irq10);
    set_idt(43, (uint64_t)irq11);
    set_idt(44, (uint64_t)irq12);
    set_idt(45, (uint64_t)irq13);
    set_idt(46, (uint64_t)irq14);
    set_idt(47, (uint64_t)irq15);

    set_idt(0x00, (uint64_t)int_00h);
    set_idt(0x01, (uint64_t)int_01h);
    set_idt(0x02, (uint64_t)int_02h);
    set_idt(0x03, (uint64_t)int_03h);
    set_idt(0x04, (uint64_t)int_04h);
    set_idt(0x05, (uint64_t)int_05h);
    set_idt(0x06, (uint64_t)int_06h);
    set_idt(0x07, (uint64_t)int_07h);
    set_idt(0x08, (uint64_t)int_08h);
    set_idt(0x09, (uint64_t)int_09h);
    set_idt(0x0A, (uint64_t)int_0Ah);
    set_idt(0x0B, (uint64_t)int_0Bh);
    set_idt(0x0C, (uint64_t)int_0Ch);
    set_idt(0x0D, (uint64_t)int_0Dh);
    set_idt(0x0E, (uint64_t)int_0Eh);
    set_idt(0x10, (uint64_t)int_10h);
    set_idt(0x11, (uint64_t)int_11h);
    set_idt(0x12, (uint64_t)int_12h);
    set_idt(0x13, (uint64_t)int_13h);
    set_idt(0x1D, (uint64_t)int_1Dh);
    set_idt(0x1E, (uint64_t)int_1Eh);
    for(int i = 0; i < 16; ++i)
    {
        irq_hook[i] = nullptr;
        irq_data[i] = nullptr;
    }
    for(int i = 0; i < 256; ++i)
    {
        exception_handler_function[i] = nullptr;
    }

    lidt(IDT, 256 * sizeof(IDTDescriptor));
    asm volatile("sti");
    return true;
}

void Interrupts::set_irq_handler(int number, void (*pFunction)(void*), void* data)
{
    if((number >= 0) && (number < 16))
    {
        irq_hook[number] = pFunction;
        irq_data[number] = data;
    }
}

void Interrupts::set_exception_handler(int number, ExceptionHandler handler)
{
    if((number >= 0) && (number < 256))
    {
        exception_handler_function[number] = handler;
    }
}
