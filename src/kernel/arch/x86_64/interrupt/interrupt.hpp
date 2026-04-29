// x86_64 interrupt descriptor table setup and callback registry. Assembly
// stubs land here before higher-level core/trap_dispatch.cpp classifies events.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/interrupt/trap_frame.hpp"

// Trap numbers
#define T_DIVIDE 0
#define T_DEBUG 1
#define T_NMI 2
#define T_BRKPT 3
#define T_OFLOW 4
#define T_BOUND 5
#define T_ILLOP 6
#define T_DEVICE 7
#define T_DBLFLT 8
#define T_TSS 10
#define T_SEGNP 11
#define T_STACK 12
#define T_GPFLT 13
#define T_PGFLT 14
#define T_FPERR 16
#define T_ALIGN 17
#define T_MCHK 18
#define T_SIMD 19
#define T_SECEV 30

#define T_IRQ0 32
#define T_SYSCALL 0x80
#define T_LTIMER 49
#define T_LERROR 50

#define IRQ_TIMER 0
#define IRQ_KBD 1
#define IRQ_SERIAL 4
#define IRQ_SPURIOUS 7
#define IRQ_IDE 14

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

using ExceptionHandler = void (*)(TrapFrame*);

class Interrupts
{
public:
    // Build the IDT and install exception/IRQ stubs.
    bool initialize();
    // Register a device IRQ callback for a legacy IRQ number.
    void set_irq_handler(int number, void (*pFunction)(void*), void* data);
    // Register a high-level exception callback for a CPU exception vector.
    void set_exception_handler(int number, ExceptionHandler handler);

private:
    IDTDescriptor IDT[256];
    // Install one IDT gate.
    void set_idt(int index, uint64_t address, uint8_t type_attr = 0x8E);
    // clear one IDT gate.
    void clear_idt(int index);
};

// Run the registered callback for one legacy IRQ vector.
void dispatch_irq_hook(int number);
// Run the registered exception callback for one CPU exception vector.
void dispatch_exception_handler(int number, TrapFrame* frame);
