// IRQ dispatch policy. Hardware-specific interrupt controller code stays in
// arch/platform modules; this layer handles timer/input side effects and thread
// selection after an IRQ.
#ifndef OS1_KERNEL_CORE_IRQ_DISPATCH_H
#define OS1_KERNEL_CORE_IRQ_DISPATCH_H

#include "arch/x86_64/interrupt/trapframe.h"

struct Thread;

// Main IRQ path called by trap dispatch for vectors T_IRQ0 through T_IRQ0+15.
Thread *HandleIrq(TrapFrame *frame);

#endif // OS1_KERNEL_CORE_IRQ_DISPATCH_H