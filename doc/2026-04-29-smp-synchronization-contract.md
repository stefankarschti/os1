# SMP Synchronization Contract - 2026-04-29

Generated-by: Codex / GPT-5, 2026-04-29.

## Current State

APs are brought online but remain parked in an interrupt-disabled idle loop. The BSP still owns live kernel execution, scheduling, process lifecycle, console input, platform discovery, PCI/device lists, and physical page allocation.

This document records the synchronization contract that applies until APs run real scheduler code.

## New Kernel Contract

The synchronization declarations live in `src/kernel/sync/smp.hpp`:

- `Spinlock`: a simple non-recursive atomic spinlock for future shared kernel state.
- `IrqGuard`: an RAII interrupt guard that disables external interrupts and restores them only if they were previously enabled.
- `OS1_BSP_ONLY`: annotation for globals that are intentionally bootstrap-CPU-owned for now.
- `KASSERT_ON_BSP()`: runtime assertion for mutation paths that are only valid on the BSP.

Current BSP-only state has been tagged in source:

- `processTable` and PID allocation in `src/kernel/proc/process.*`
- `threadTable`, TID allocation, idle-thread publication, and runnable links in `src/kernel/proc/thread.*`
- `terminal[]` and `active_terminal` in `src/kernel/core/kernel_state.*`
- console input queues in `src/kernel/console/console_input.cpp`
- PCI/platform discovery state in `src/kernel/platform/state.*`
- `page_frames` in `src/kernel/core/kernel_state.*` and allocator mutation paths in `src/kernel/mm/page_frame.cpp`

## Locking Order

When APs stop being parked, lock acquisition must follow this order. Do not introduce a new lock outside this order without updating this document first.

1. CPU-local interrupt state: acquire `IrqGuard` before taking any lock that can also be touched from an interrupt path.
2. Physical memory allocator: `page_frames` lock.
3. Virtual memory/address-space ownership locks.
4. Process table lock.
5. Thread table and scheduler/run-queue locks.
6. Wait/IPC/event queues.
7. Console input queue lock.
8. Terminal/display locks.
9. Platform/PCI/device-list locks.
10. Individual driver/device locks.

Rules:

- Never allocate physical pages while holding a driver/device lock.
- Never call into console or debug output while holding the process, thread, scheduler, platform, or allocator lock unless the path is explicitly panic-only.
- Never sleep or block while holding a spinlock.
- Never acquire a lower-numbered lock while holding a higher-numbered lock.
- Interrupt handlers may only take locks that are IRQ-safe and must do so under `IrqGuard`.
- Locks protect data ownership; they do not replace lifetime rules. Process, thread, page-table, and device lifetimes still need explicit ownership transitions.

## Migration Path

Before APs enter the normal scheduler:

1. Replace every `OS1_BSP_ONLY` global with either a lock, CPU-local ownership, immutable publication, or single-writer handoff rule.
2. Convert `KASSERT_ON_BSP()` assertions into lock assertions or remove them only after the relevant state is protected.
3. Add lock-order assertions if nested locks become common.
4. Add host tests for pure lock-order helpers if such helpers are introduced.
5. Add QEMU SMP smoke coverage that proves APs can run user threads without corrupting process/thread/page-frame state.

Until that work is done, `KASSERT_ON_BSP()` failures are correctness bugs, not diagnostic noise.
