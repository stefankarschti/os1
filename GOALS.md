# GOALS

This document captures the long-term direction of `os1`. For the concrete snapshot of what exists today, see [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) (which now includes a system diagram and end-to-end workflow). For the build, run, and smoke workflow, see [README.md](README.md). For a full code-grounded project review, see [doc/2026-04-23-review.md](doc/2026-04-23-review.md). For the most recent completed operator-environment design, see [doc/2026-04-23-milestone-5-interactive-shell-and-observability.md](doc/2026-04-23-milestone-5-interactive-shell-and-observability.md).

## Vision

`os1` is a self-documented teaching and engineering operating system project.

The aim is **not** to become a production general-purpose operating system. The aim is to build a clean, understandable, technically serious OS whose source code demonstrates the major mechanisms of a modern OS through a small, coherent system.

The project should favor:

- clarity over cleverness
- explicit architecture over accidental complexity
- strong documentation near the code
- incremental progress with runnable milestones
- modern OS concepts over nostalgia-driven dead ends
- complete vertical slices over scattered low-level experiments

The project should remain small enough to understand, but ambitious enough to cover the core building blocks of a modern system.

## Primary objectives

### 1. Educational value through source code

The source code should be readable, structured, and documented where it matters.

Goals:

- make subsystem boundaries clear
- document invariants and assumptions
- explain why a design was chosen, not only what it does
- keep low-level code auditable
- support learning by reading the source tree and docs
- preserve important bring-up knowledge in architecture notes and milestone reviews

### 2. Meaningful modern-OS feature coverage

`os1` should eventually demonstrate most major OS areas, including:

- boot and platform initialization
- physical and virtual memory management
- interrupts and exceptions
- task scheduling and process management
- user/kernel separation
- executable loading
- filesystems and storage abstraction
- device drivers
- networking
- multiuser security model
- remote access
- optional graphical desktop layer

This does **not** mean full parity with Linux, BSD, or Windows. It means meaningful coverage of the concepts in a system that still fits in one person’s head.

### 3. Simplicity first

When several approaches are possible, prefer the one that:

- is easier to reason about
- is easier to debug
- keeps long-term architecture clean
- does not block future extension

Avoid premature optimization, speculative abstractions, and feature creep.

### 4. Runnable vertical slices

The project should be developed through complete milestones rather than disconnected technical experiments.

Examples:

- boot to terminal
- load and run a user program in user mode
- mount a filesystem and run a shell
- obtain network connectivity
- support remote login through SSH
- optionally launch a simple graphical desktop

A narrow but complete milestone is better than broad unfinished infrastructure.

## Scope and platform goals

### Primary architecture

- Primary target: `x86_64`
- The design should avoid unnecessary assumptions that would prevent later ports.

### Future architecture portability

Keep the door open for later support of:

- `AArch64`
- `RISC-V`

Portability should be enabled by clean architecture boundaries, not by forcing premature cross-architecture generalization everywhere.

Practical rule:

- architect for portability early
- implement portability only when justified

### Boot strategy

Current path:

- Limine plus UEFI is the default boot path
- the legacy BIOS raw image remains in-tree as a compatibility path and continuous-test target

Long-term rule:

- keep the kernel bootloader-agnostic through the shared `BootInfo` contract
- keep BIOS support only while it remains cheap and architecturally clean
- avoid reintroducing bootloader-specific assumptions into kernel code once they have been normalized away

Design rule:

- bootloader- or firmware-specific logic should hand off into a common kernel boot interface
- the kernel should not become tightly coupled to a single boot path if avoidable
- BIOS compatibility should not dominate long-term architecture decisions now that the UEFI path exists

### Platform discovery

The project should support modern machine discovery on `x86_64`, including:

- memory map handoff
- ACPI discovery and parsing
- interrupt-controller discovery
- timer discovery
- PCI/PCIe enumeration
- virtio-first VM devices, with `virtio-blk` as the first practical storage path

### Development target environments

Near-term development should optimize for:

- QEMU reproducibility first
- virtio-first virtual-device support where applicable
- clean serial-debug workflow
- predictable virtual hardware targets

Real-hardware bring-up matters, but not before the architecture is coherent enough to justify it.

## User-facing system goals

### Terminal-first operating system

The main system should first become a usable terminal-oriented OS.

Core expectations:

- text terminal interface
- shell and command execution
- user accounts
- file permissions
- remote login

### Networking

Networking is a first-class goal, not an afterthought.

Initial network goals:

- network driver model
- IPv4 support
- basic TCP/IP stack
- DHCP or static addressing
- DNS resolver support
- remote shell and file transfer primitives

### SSH and remote administration

A major usability goal is remote login.

Target capability:

- log into `os1` remotely over SSH

Implications:

- user authentication
- privilege separation
- process isolation
- permissions model
- enough network stack maturity to support a secure remote session
- pseudo-terminal and session-management support eventually

SSH is a **late-mid-stage goal**, not an early bring-up feature.

### Optional desktop variant

After the terminal-oriented system is coherent, `os1` may grow an optional home-made GUI / desktop mode.

This GUI is explicitly secondary to the core OS.

The first GUI target should be a **framebuffer terminal compositor**, not a general desktop environment.

Goals for the desktop variant:

- simple framebuffer or compositor-based UI
- built-in terminal windows or panes
- basic input handling
- minimal windowing or pane management
- simple native widgets only if justified

Non-goal:

- competing with mature desktop environments

## System architecture goals

### Kernel model

The project should remain **monolithic at first**.

Reasoning:

- simpler to understand and debug
- better fit for a one-person hobby OS
- avoids premature complexity around module loading and ABI stability

Later modularization is acceptable where it clearly improves structure, but early design should not pretend that a microkernel or plugin-heavy model is free.

### Kernel and userland separation

The project has already crossed the first protected-userland threshold: it boots through a minimal `/bin/init` that immediately `exec`s `/bin/sh`, can load statically linked user ELF programs from an initrd, services a small but real syscall ABI, and can kill or reap a faulting child process without panicking the kernel. The remaining goal is to deepen that foundation into a more complete operating system model.

Required eventual capabilities:

- filesystem-backed executable loading and launch policy
- stronger process and thread lifecycle management
- broader descriptor, IPC, and resource-access models
- arguments, environment passing, and richer program-loading semantics
- filesystem-backed program and data access
- a multiuser environment beyond the current initrd shell and companion programs

Protected userland remains one of the most important architectural pillars in the project.

### Process and scheduling model

The system should support:

- multitasking
- per-process isolation
- scheduler with clear policy
- early SMP-aware design
- eventual full user-process SMP scheduling across CPUs

SMP should come early enough to shape:

- per-CPU data structures
- interrupt routing assumptions
- locking strategy
- scheduler design
- tracing / observability design

The implementation may still progress through simpler intermediate steps, but the architecture should not assume that single-core is the long-term model.

### Security model

The project should support a simple but real security foundation.

Goals:

- multiuser model
- user/group identity
- file ownership and permissions
- privilege boundaries
- least-privilege design where practical
- secure enough architecture to justify SSH and remote administration

Non-goal:

- enterprise-grade hardening in early stages

### Storage and filesystems

The system should support persistent storage with a simple, understandable filesystem story.

Suggested progression:

- initrd or in-memory filesystem early
- simple native filesystem or readable existing simple filesystem later
- block-device abstraction
- partition awareness later if justified

### Driver model

The system should gradually move from emulator- and board-specific code toward a clearer device-driver model.

Likely priorities:

- console
- timer
- keyboard
- storage
- network
- framebuffer
- interrupt-controller and SMP-enabling platform devices
- early accelerator / GPU discovery and compute-oriented interfaces where available

Early accelerator work should start with **discovery only**. Later, this may evolve toward actual compute submission on a virtual or real device, including AI-oriented experimentation, but the early goal is to avoid painting the system into a CPU-only corner.

## Engineering goals

### Documentation quality

Documentation should exist at several levels:

- high-level architecture documents
- per-subsystem design notes
- code comments for invariants and tricky low-level behavior
- milestone reviews / retrospectives
- clear notes on current state vs target state

### Testability and observability

The system should become easier to inspect and debug over time.

Suggested goals:

- serial logging
- structured tracing where justified
- deterministic emulator workflows
- debug build modes
- subsystem-specific test harnesses where practical
- milestone demo scripts

### Performance stance

Performance matters, but not at the expense of clarity in early stages.

Rules:

- do not sacrifice architecture for trivial micro-optimizations
- do not introduce high-level runtime dependencies that weaken control over the system
- optimize deliberately after measurement

### Language stance

Preferred implementation language:

- C++ as the main systems language

Acceptable supporting languages:

- assembly where required for boot, interrupts, context switching, and CPU-specific entry paths
- limited C or other targeted low-level components only when clearly justified

Rule:

- the language mix must serve clarity, control, and performance
- language experimentation is not a goal by itself

## Non-goals

The project is not trying to:

- become Linux-compatible in the short term
- support all hardware quickly
- maximize feature count at the expense of coherence
- become GUI-first before the terminal system is solid
- chase novelty with weak fundamentals
- preserve legacy BIOS as a first-class long-term identity

## Recommended sequencing

The existing review is directionally correct on the importance of protected userland: this project explicitly prioritizes **running isolated user programs** over early broad real-hardware support. However, the architecture should be shaped early by **SMP readiness** and eventual accelerator / heterogeneous-compute support, even when some capabilities are initially stubbed or emulator-first.

In other words:

- protected userland outranks early real-hardware breadth
- SMP should influence kernel structure early, not be bolted on late
- GPU / accelerator compute should be considered a first-class future subsystem early enough that memory, scheduling, driver, and security boundaries do not assume a CPU-only world

The in-repo milestone designs (M1–M5) refine the coarser A–G map below into concrete engineering plans.

### Milestone A: Clean boot and kernel baseline *(implemented — see [M1 design](doc/2026-04-22-milestone-1-boot-contract-and-kernel-stabilization.md))*

- reliable boot in emulator
- documented boot flow via a versioned `BootInfo` handoff
- basic memory and interrupt setup
- serial output and terminal output
- centralized early-boot addresses, AP idle state, C++20 kernel baseline, headless QEMU smoke test in CI

### Milestone B: Modern platform handoff *(partially implemented — M1 covers the contract; M3 + M4 deliver the modern boot and discovery paths)*

- common boot information handoff structure (*`BootInfo` — implemented*)
- UEFI path ([M3](doc/2026-04-22-milestone-3-modern-default-boot-path.md))
- framebuffer support (M3 handoff; compositor later)
- ACPI discovery ([M4](doc/2026-04-22-milestone-4-modern-platform-support.md) — implemented)
- improved platform abstraction (M4 — implemented)
- PCIe enumeration and first `virtio-blk` transport ([M4](doc/2026-04-22-milestone-4-modern-platform-support.md) — implemented)
- early APIC / SMP-oriented platform groundwork (per-CPU `cpu` pages, TSS, ACPI-derived AP bring-up, and AP idle state are implemented; SMP scheduling is still a follow-on)
- early accelerator / GPU device discovery path where feasible

### Milestone C: Protected userland and operator shell baseline *(implemented across [M2 design](doc/2026-04-22-milestone-2-process-model-and-isolation.md) and [M5 design](doc/2026-04-23-milestone-5-interactive-shell-and-observability.md))*

- ring-3 user-mode execution
- ELF64 executable loading from a cpio-newc initrd
- a small `int 0x80` syscall interface for console I/O, observability, and initrd-backed process control
- initrd-backed operator environment with `/bin/init`, `/bin/sh`, `/bin/yield`, and `/bin/fault`
- serial-drivable shell and smoke coverage on both boot paths

Later follow-ups expected in this area: filesystem-backed loading, richer file-descriptor semantics, arguments/environment support, and an optional fast-syscall (`SYSCALL`/`SYSRET`) path.

### Milestone D: Persistence and local security *(next)*

The virtio-blk probe in Milestone 4 is a smoke path, not a block layer. Milestone D therefore has two halves that should not be conflated:

1. a narrow in-kernel `BlockDevice` abstraction above `virtio-blk`, followed by a first read-only filesystem (FAT32 or a bespoke simple FS) and filesystem-backed `exec`;
2. a small multiuser/permissions foundation (uid/gid, file ownership, user table) so Milestone F can meaningfully run SSH.

Implementation notes grounded in current source:

- grow the current `BlockDevice` facade in [src/kernel/storage/block_device.hpp](src/kernel/storage/block_device.hpp) and [src/kernel/drivers/block/virtio_blk.cpp](src/kernel/drivers/block/virtio_blk.cpp) from polling smoke coverage into request ownership, completion, errors, and eventually write support
- move `virtio-blk` to MSI/MSI-X *before* the filesystem layer is written on top
- add argv/envp handoff in the initrd-backed loader under [src/kernel/proc/user_program.cpp](src/kernel/proc/user_program.cpp) so a real `init` can evolve apart from `/bin/sh`

### Milestone E: Networking foundation

- NIC support
- IP networking
- TCP basics
- remote shell building blocks

### Milestone F: SSH-capable remote administration

- secure login path
- session management
- pseudo-terminals
- enough security maturity to justify remote exposure

### Milestone G: Optional desktop layer

- framebuffer terminal compositor
- terminal windows or panes
- keyboard and pointing-device support
- optional evolution toward a small desktop shell later

## Open design choices

Some earlier open questions have since been resolved in source or in the milestone designs; they are listed here for the record rather than as open work:

- **Resolved:** the kernel-facing boot contract is a single versioned `BootInfo` block normalized by each boot source (see [M1](doc/2026-04-22-milestone-1-boot-contract-and-kernel-stabilization.md) and [src/kernel/handoff/boot_info.hpp](src/kernel/handoff/boot_info.hpp)).
- **Resolved:** the modern default boot path is Limine plus UEFI, while BIOS remains available during the transition ([M3](doc/2026-04-22-milestone-3-modern-default-boot-path.md)).
- **Resolved:** the first protected-userland ABI is statically linked ELF64 / `ET_EXEC` loaded from a `cpio newc` initrd, with `int 0x80` syscalls matching the System V AMD64 register layout and now covering console I/O, observability, and initrd-backed process control ([M2](doc/2026-04-22-milestone-2-process-model-and-isolation.md), [M5](doc/2026-04-23-milestone-5-interactive-shell-and-observability.md), [src/uapi/os1/syscall_numbers.h](src/uapi/os1/syscall_numbers.h), [src/kernel/syscall/abi.hpp](src/kernel/syscall/abi.hpp), [src/uapi/os1/observe.h](src/uapi/os1/observe.h), [src/user/](src/user/)).
- **Resolved:** the first operator environment is an initrd-backed ring-3 shell entered through a minimal `/bin/init` that `exec`s `/bin/sh`, scriptable through serial input and backed by explicit kernel observability snapshots rather than parsed boot logs ([M5](doc/2026-04-23-milestone-5-interactive-shell-and-observability.md), [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md)).

The following are not fully decided yet and should be revisited explicitly:

- first filesystem choice
- first NIC/device targets in QEMU / virtio-first environments beyond the initial `virtio-blk` + `virtio-net` set proposed in [M4](doc/2026-04-22-milestone-4-modern-platform-support.md)
- staging plan from AP startup (currently: APs run `cpu_idle_loop()`) to full user-process SMP scheduling across CPUs
- later GPU / accelerator target model after discovery-only phase: compute queues, minimal kernel offload primitives, or richer user-facing submission model
- whether the terminal compositor stays intentionally minimal or grows into a broader desktop shell
- whether to keep `int 0x80` as the permanent syscall entry or migrate to `SYSCALL`/`SYSRET` once the current userland matures

## Additional recommended goals

These are strong candidates to adopt explicitly:

- **serial-first debugging:** always preserve a reliable non-graphical debug path
- **QEMU-first developer workflow:** every major milestone should be reproducible in emulation
- **deterministic milestone demos:** each milestone should have a short demonstration script
- **architecture isolation:** keep `arch/x86_64` concerns separate from generic kernel code
- **clear trust boundaries:** document privileged vs unprivileged code paths early
- **bring-up notebooks / logs:** preserve decisions, dead ends, and lessons learned
- **target-state honesty:** clearly label what is current, what is experimental, and what is aspirational
- **SMP observability:** make per-CPU behavior and cross-CPU events inspectable early
- **accelerator awareness:** keep memory, scheduling, and driver abstractions compatible with eventual GPU / accelerator compute support

## Short project thesis

`os1` should become a clean, well-documented, terminal-first operating system for `x86_64` that demonstrates the major mechanisms of a modern OS, prioritizes protected userland over early hardware breadth, adopts monolithic-kernel and QEMU/virtio-first development early, grows toward full SMP user-process scheduling, secure multiuser networking, and remote login, and optionally supports a simple home-made framebuffer terminal compositor later without compromising core system clarity.
