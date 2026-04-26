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

#include <stdint.h>

void ioapic_init(void);

void ioapic_set_primary(uint32_t gsi_base);
bool ioapic_enable_gsi(uint32_t gsi, int irq, uint16_t flags);
void ioapic_enable(int intin, int irq = -1);

#endif // _ioapic_h_
