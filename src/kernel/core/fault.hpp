// Fault and exception handling. User faults terminate the current user thread;
// kernel faults are reported through serial/terminal output and halt the CPU.
#pragma once

#include <stdint.h>

#include "arch/x86_64/interrupt/trap_frame.hpp"

struct Thread;

// Return a human-readable architectural name for an exception vector.
const char* kernel_fault_name(uint64_t vector);

// Dump a trap frame to the serial debug channel.
void dump_trap_frame(const TrapFrame& frame);

// Exception callback registered for kernel-mode exception vectors.
void on_kernel_exception(TrapFrame* frame);

// Main exception path called by trap dispatch.
Thread* handle_exception(TrapFrame* frame);
