// Kernel idle-thread entry point. The current system has one idle thread; this
// boundary is where per-CPU idle behavior can grow when AP scheduling arrives.
#ifndef OS1_KERNEL_SCHED_IDLE_H
#define OS1_KERNEL_SCHED_IDLE_H

// Entry function for the scheduler's idle thread.
void KernelIdleThread();

#endif // OS1_KERNEL_SCHED_IDLE_H