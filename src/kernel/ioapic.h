/*
 * I/O APIC (Advanced Programmable Interrupt Controller) definitions.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from xv6 and FreeBSD code.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */
#ifndef _ioapic_h_
#define _ioapic_h_

void ioapic_init(void);

void ioapic_enable(int irq);

#endif // _ioapic_h_
