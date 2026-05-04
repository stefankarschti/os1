// ISA IRQ routing through normalized platform interrupt-override state.
#pragma once

#include <stdint.h>

#include "platform/types.hpp"

// Add one legacy ISA override record to platform state.
bool add_legacy_interrupt_override(uint8_t bus_irq, uint32_t global_irq, uint16_t flags);

// Route one ISA IRQ line to one explicit interrupt vector.
bool platform_route_isa_irq(DeviceId owner, int bus_irq, uint8_t vector);

// Route one exact GSI to one explicit interrupt vector.
bool platform_route_gsi_irq(DeviceId owner, uint32_t gsi, uint16_t flags, uint8_t vector);
