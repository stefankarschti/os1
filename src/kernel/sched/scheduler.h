// Scheduler handoff helpers extracted from trap and syscall paths. Process and
// thread lifetime lives in proc/; this layer only chooses the next runnable
// thread and handles blocking/death wakeups at scheduling boundaries.
#ifndef OS1_KERNEL_SCHED_SCHEDULER_H
#define OS1_KERNEL_SCHED_SCHEDULER_H

struct Thread;

// Choose the next thread to run, optionally preserving the current thread as
// runnable before selection.
[[nodiscard]] Thread *ScheduleNext(bool keep_current);

#endif // OS1_KERNEL_SCHED_SCHEDULER_H