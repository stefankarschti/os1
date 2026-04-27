// Final kernel stop path. Keeping this tiny boundary named makes later panic
// reporting easier without scattering halt loops across exception handlers.
#ifndef OS1_KERNEL_CORE_PANIC_H
#define OS1_KERNEL_CORE_PANIC_H

// Disable interrupts and halt forever.
[[noreturn]] void HaltForever();

#endif // OS1_KERNEL_CORE_PANIC_H