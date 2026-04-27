/*
 * I/O APIC (Advanced Programmable Interrupt Controller) definitions.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from xv6 and FreeBSD code.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */
#pragma once

#include <stdint.h>

// initialize the primary IOAPIC using discovered platform state.
void ioapic_init(void);

// Record the global-system-interrupt base for the primary IOAPIC.
void ioapic_set_primary(uint32_t gsi_base);
// Enable a platform GSI with ACPI polarity/trigger flags.
bool ioapic_enable_gsi(uint32_t gsi, int irq, uint16_t flags);
// Enable a legacy INTIN line with the default active-high edge-triggered policy.
void ioapic_enable(int intin, int irq = -1);
