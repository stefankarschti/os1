// Narrow helpers for x86_64 control registers used by fault reporting,
// scheduler handoff, and syscall code that temporarily switches CR3.
#pragma once

#include <stdint.h>

// Read CR2, which holds the faulting linear address after a page fault.
[[nodiscard]] inline uint64_t read_cr2()
{
    uint64_t value = 0;
    asm volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

// Read the currently active page-table root from CR3.
[[nodiscard]] inline uint64_t read_cr3()
{
    uint64_t value = 0;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

// Install a new page-table root into CR3 and flush non-global translations.
inline void write_cr3(uint64_t value)
{
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}
