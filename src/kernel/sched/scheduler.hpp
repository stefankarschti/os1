// Scheduler handoff helpers extracted from trap and syscall paths. Process and
// thread lifetime lives in proc/; this layer only chooses the next runnable
// thread and handles blocking/death wakeups at scheduling boundaries.
#pragma once

struct Thread;

// Choose the next thread to run, optionally preserving the current thread as
// runnable before selection.
[[nodiscard]] Thread *schedule_next(bool keep_current);

