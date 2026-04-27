// Fault and exception handling. User faults terminate the current user thread;
// kernel faults are reported through serial/terminal output and halt the CPU.
#ifndef OS1_KERNEL_CORE_FAULT_H
#define OS1_KERNEL_CORE_FAULT_H

#include <stdint.h>

#include "arch/x86_64/interrupt/trapframe.h"

struct Thread;

// Return a human-readable architectural name for an exception vector.
const char *KernelFaultName(uint64_t vector);

// Dump a trap frame to the serial debug channel.
void DumpTrapFrame(const TrapFrame &frame);

// Exception callback registered for kernel-mode exception vectors.
void OnKernelException(TrapFrame *frame);

// Main exception path called by trap dispatch.
Thread *HandleException(TrapFrame *frame);

#endif // OS1_KERNEL_CORE_FAULT_H