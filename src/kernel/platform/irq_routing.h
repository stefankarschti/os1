// ISA IRQ routing through normalized platform interrupt-override state.
#ifndef OS1_KERNEL_PLATFORM_IRQ_ROUTING_H
#define OS1_KERNEL_PLATFORM_IRQ_ROUTING_H

#include <stdint.h>

// Add one legacy ISA override record to platform state.
bool AddLegacyInterruptOverride(uint8_t bus_irq, uint32_t global_irq, uint16_t flags);

#endif // OS1_KERNEL_PLATFORM_IRQ_ROUTING_H