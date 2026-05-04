// BSP-owned interrupt-vector allocator for future MSI/MSI-X and APIC-local
// device vectors. It only manages the dynamic vector window, not exceptions,
// syscalls, or legacy ISA IRQ slots.
#pragma once

#include <stdint.h>

#include "arch/x86_64/interrupt/interrupt.hpp"

// Clear all dynamic-vector allocations. Call during interrupt initialization.
void irq_vector_allocator_reset();

// Return true when the vector is within the dynamic allocatable window.
[[nodiscard]] bool irq_vector_is_allocatable(uint8_t vector);

// Return true when the vector is currently allocated.
[[nodiscard]] bool irq_vector_is_allocated(uint8_t vector);

// Allocate one free dynamic vector.
bool irq_allocate_vector(uint8_t& vector);

// Reserve one specific dynamic vector.
bool irq_reserve_vector(uint8_t vector);

// Release one allocated dynamic vector.
bool irq_free_vector(uint8_t vector);
