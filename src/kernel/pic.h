/*
 * Hardware definitions for the 8259A Programmable Interrupt Controller (PIC).
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#ifndef _pic_h_
#define _pic_h_

#define MAX_IRQS	16	// Number of IRQs

// I/O Addresses of the two 8259A programmable interrupt controllers
#define IO_PIC1		0x20	// Master (IRQs 0-7)
#define IO_PIC2		0xA0	// Slave (IRQs 8-15)

#define IRQ_SLAVE	2	// IRQ at which slave connects to master

#include "stdint.h"
#include "stddef.h"
#include "x86.h"

extern uint16_t irq_mask_8259A;

void pic_init(void);
void pic_setmask(uint16_t mask);
void pic_enable(int irq);

#endif // _pic_h_
