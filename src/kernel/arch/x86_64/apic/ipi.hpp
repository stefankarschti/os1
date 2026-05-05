// Local APIC interrupt-command helpers for kernel IPIs.
#pragma once

#include <stdint.h>

[[nodiscard]] bool ipi_send(uint8_t apic_id, uint8_t vector);
[[nodiscard]] bool ipi_send_all_but_self(uint8_t vector);
