// Legacy PIT timer programming. Scheduler policy is outside this driver; this
// file only owns the hardware divisor setup.
#pragma once

#include <stdint.h>

// Program the PIT near `frequency` Hz and return the effective rate.
uint16_t set_timer(uint16_t frequency);
