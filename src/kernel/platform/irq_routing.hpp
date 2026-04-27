// ISA IRQ routing through normalized platform interrupt-override state.
#pragma once

#include <stdint.h>

// Add one legacy ISA override record to platform state.
bool add_legacy_interrupt_override(uint8_t bus_irq, uint32_t global_irq, uint16_t flags);
