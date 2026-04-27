// Top-level syscall ABI dispatch. Individual syscall bodies live in smaller
// files so this module only translates register state into explicit contexts.
#ifndef OS1_KERNEL_SYSCALL_DISPATCH_H
#define OS1_KERNEL_SYSCALL_DISPATCH_H

#include "arch/x86_64/interrupt/trapframe.h"

struct Thread;

// Handle one syscall trap and return the thread that should resume next.
Thread *HandleSyscall(TrapFrame *frame);

#endif // OS1_KERNEL_SYSCALL_DISPATCH_H