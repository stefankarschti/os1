// Legacy PIT timer programming. Scheduler policy is outside this driver; this
// file only owns the hardware divisor setup.
#ifndef OS1_KERNEL_DRIVERS_TIMER_PIT_H
#define OS1_KERNEL_DRIVERS_TIMER_PIT_H

#include <stdint.h>

// Program the PIT near `frequency` Hz and return the effective rate.
uint16_t SetTimer(uint16_t frequency);

#endif // OS1_KERNEL_DRIVERS_TIMER_PIT_H