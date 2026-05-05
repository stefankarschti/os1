# SMP Enablement Implementation Review - 2026-05-05

Source checked against [2026-05-05-phased-smp-enablement.md](2026-05-05-phased-smp-enablement.md)
using the recommended per-CPU run-queue/load-balancer option.

## Findings

1. **BIOS balance smoke is not deterministic.**

   `smoke_balance_bios` can time out after spawning all 16 balance workers without
   ever printing `[user/balancecheck] runq delta=` or the shell completion marker.
   The smoke program waits for every worker to complete, and each worker yields
   65,536 times. That is fine for UEFI on this machine but too slow for the BIOS
   path under the current 30s smoke ceiling.

   Fix: shorten the balance worker loop to keep the smoke focused on observing
   queue convergence, not on long post-observation child runtime.

2. **AP timer smoke evidence is incomplete.**

   The plan requires smoke coverage for AP timer ticks. The implementation has
   `OS1_KERNEL_EVENT_AP_TICK`, but it is emitted only every 1000 AP-local ticks,
   and the observe smokes do not assert it. Current observe logs show AP online,
   run-queue depth, kernel ping, and reschedule IPI events, but not AP tick events
   before the event-ring dump.

   Fix: emit the first AP tick per CPU immediately, keep the 1000-tick
   rate-limit afterward, and add `event ap-tick seq=` to UEFI/BIOS observe smoke
   markers.

3. **Console-read waiters still use a registry walk instead of a wait queue.**

   Phase 5 calls for console read waits to use a console-owned `WaitQueue`.
   `block_current_thread_on_console_read` still blocks without a queue, and
   `wake_console_readers` still finds waiters through
   `first_blocked_thread(ThreadWaitReason::ConsoleRead)`. This keeps a
   scheduler hot path tied to global thread-registry iteration.

   Fix: add a console-read `WaitQueue` owned by `console_input`, enqueue console
   readers there, and wake from that queue.

4. **`observe processes` violates the documented lock order.**

   `sys_observe_processes` takes `g_thread_registry_lock` before
   `g_process_table_lock`, while the SMP contract orders process table before
   thread registry. The count pass also reads `entry->process` while holding only
   the thread lock.

   Fix: take process table then thread registry in both passes.

5. **Some named SMP smoke targets from the plan are consolidated rather than
   present as standalone targets.**

   The current `smoke_observe` and `smoke_balance` targets cover AP online, AP
   idle, kernel ping, user work, and balance behavior, but the plan's individual
   `smoke_smp_*` target names are not present. This is acceptable for now if the
   consolidated smokes assert the same observables; AP tick was the missing
   observable from that set.

## Plan

1. Convert console-read blocking/wakeup to the console-owned wait queue.
2. Correct `sys_observe_processes` lock ordering.
3. Emit and assert early AP tick events.
4. Shorten the balance worker loop so the BIOS balance smoke completes within
   the existing timeout.
5. Re-run host tests and `smoke_all`; iterate until both pass.
