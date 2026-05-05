// Scheduler handoff helpers extracted from trap and syscall paths. Process and
// thread lifetime lives in proc/; this layer only chooses the next runnable
// thread and handles blocking/death wakeups at scheduling boundaries.
#pragma once

#include <stdint.h>

struct cpu;
struct Thread;

// Choose the next thread to run, optionally preserving the current thread as
// runnable before selection.
[[nodiscard]] Thread* schedule_next(bool keep_current);

// Run one load-balancing attempt for `target_cpu` at `now` ticks.
[[nodiscard]] bool scheduler_balance_cpu(cpu* target_cpu, uint64_t now, bool force = false);

// Trigger periodic or idle-path balancing on the current CPU after a timer tick.
void scheduler_handle_timer_tick();
