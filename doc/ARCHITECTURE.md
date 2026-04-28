# os1 Architecture

> generated-by: GitHub Copilot - generated-at: 2026-04-27 - git-state: working tree

This document is the current-state source of truth for `os1`. It describes what is implemented in the repository today. For build, run, and smoke workflows, see [README](../README.md). For the longer-term direction, see [GOALS](../GOALS.md). For the external specifications, firmware manuals, ABI references, and protocol standards that inform the project, see [REFERENCES](REFERENCES.md). For the shell/operator milestone that produced the current user-facing environment, see [Milestone 5 Design: Interactive Shell And Observability](2026-04-23-milestone-5-interactive-shell-and-observability.md). For a full code-grounded review of the project, see [Latest Review](latest-review.md). The review documents under `doc/` are historical context, not the live system contract.

`os1` currently has:

- a shared-kernel, dual-entry boot architecture
- a default modern UEFI boot path based on Limine
- an explicit legacy BIOS compatibility path
- one kernel-facing boot contract: `BootInfo`
- an ACPI-first platform-discovery layer on both boot frontends
- PCIe enumeration through ACPI `MCFG` and ECAM
- a first practical modern device path through `virtio-blk`
- a freestanding `C++20` kernel core with narrow assembly boundaries
- a source tree split by major responsibility: `arch/x86_64`, `core`, `handoff`, `mm`, `proc`, `sched`, `syscall`, `console`, `drivers`, `platform`, `storage`, `fs`, `vfs`, `security`, `debug`, `util`, and `linker`
- protected ring-3 user programs loaded from an initrd
- a terminal-first operator environment that boots directly into a ring-3 shell
- structured read-only observability snapshots exposed through a shared UAPI
- initrd-backed `spawn` / `waitpid` / `exec` process control for user commands
- a terminal model that can render either through VGA text mode or a framebuffer text backend
- serial-driven smoke coverage and interactive serial run targets for both the UEFI and BIOS paths

Milestone status:

- [Milestone 1: Boot Contract And Kernel Stabilization](2026-04-22-milestone-1-boot-contract-and-kernel-stabilization.md): implemented
- [Milestone 2: Process Model And Isolation](2026-04-22-milestone-2-process-model-and-isolation.md): implemented
- [Milestone 3: Modern Default Boot Path](2026-04-22-milestone-3-modern-default-boot-path.md): implemented
- [Milestone 4: Modern Platform Support](2026-04-22-milestone-4-modern-platform-support.md): implemented
- [Milestone 5: Interactive Shell And Observability](2026-04-23-milestone-5-interactive-shell-and-observability.md): implemented

## Glossary

| Term | Meaning in `os1` |
| --- | --- |
| BIOS | Legacy PC firmware interface used by the compatibility boot path. |
| UEFI | Modern firmware interface used by the default boot path. |
| Limine | The bootloader / protocol used for the modern default path. |
| OVMF | The UEFI firmware image used by QEMU for the default `run` and `smoke` targets. |
| HHDM | Higher-Half Direct Map. Limine provides a virtual mapping of physical memory so the shim can access physical ranges before the kernel installs its own page tables. |
| `BootInfo` | The normalized bootloader-to-kernel contract. Every boot frontend must convert its native state into this structure before entering `kernel_main`. |
| Framebuffer | A physical memory region whose bytes directly represent display pixels. |
| `cpio newc` | The archive format used for the initrd. It contains the first user-space programs. |
| `ET_EXEC` | The ELF executable type used by the current statically linked user programs. |
| PML4 / PML3 / PML2 / PML1 | The four levels of x86_64 page tables. |
| `RSP0` | The kernel stack pointer stored in the Task State Segment. The CPU loads it automatically when an interrupt or syscall enters the kernel from ring 3. |
| `TrapFrame` | The saved register/state layout shared by exceptions, IRQs, syscalls, and scheduler return paths. |

## System Shape

The system is deliberately split into two layers:

1. Thin boot frontends.
2. One shared kernel core.

The boot frontends are allowed to differ because firmware and bootloader requirements differ. The kernel core is not.

The current frontends are:

- `src/boot/bios/` building `kernel16.bin` plus `boot.bin` for the legacy BIOS raw-image path
- `src/boot/limine/` building `kernel_limine.elf` for the default Limine/UEFI path

The shared kernel core is:

- `kernel.elf`

It is the low-half kernel core used by both paths. The Limine path loads it as a module and then transfers control into the same `kernel_main(BootInfo*, cpu*)` entry that the BIOS loader uses.

This split exists for a pragmatic reason: the kernel core is still linked at low identity-mapped addresses around `0x00100000`, while the Limine executable itself must be presented in a form the modern bootloader accepts. The higher-half Limine frontend exists to bridge that difference without teaching the kernel multiple boot ABIs.

## Source Tree Ownership

This section is the live source-structure contract for the kernel. Dated refactor notes and review docs are historical context; when they conflict with this section, this document owns the current layout.

Top-level kernel source rules:

- The `src/kernel/` top level currently keeps two loose ownership files: [../src/kernel/CMakeLists.txt](../src/kernel/CMakeLists.txt) for build grouping.
- C++ sources are grouped in CMake by ownership: architecture, handoff, memory, console, drivers, filesystem, core, platform, process, scheduler, syscall, debug, and utilities.
- NASM include paths explicitly include architecture layout files, handoff layout files, and process thread-layout files so assembly does not rely on old flat-tree placement.
- Internal C++ headers use `.hpp` and `#pragma once`; the remaining `.h` headers are deliberate C/UAPI/layout contracts.
- Future-growth directories exist with ownership notes even before executable code lands, so later work has an obvious home.

Current kernel folders:

| Folder | Role | Current important files |
| --- | --- | --- |
| [../src/kernel/core/](../src/kernel/core) | Shared kernel orchestration, trap classification, IRQ dispatch, fault policy, panic/halt, and temporary global kernel state. | `kernel_main.cpp`, `kernel_state.hpp`, `trap_dispatch.cpp`, `irq_dispatch.hpp`, `fault.hpp`, `panic.hpp` |
| [../src/kernel/handoff/](../src/kernel/handoff) | Bootloader-to-kernel ABI and fixed early memory layout shared by BIOS, Limine, C++, and NASM. | `boot_info.hpp`, `boot_info.cpp`, `memory_layout.h`, `memory_layout.inc` |
| [../src/kernel/arch/x86_64/](../src/kernel/arch/x86_64) | x86_64 CPU, APIC, interrupt, assembly, and processor helper code. Generic kernel code should include the narrowest architecture header it needs. | `cpu/`, `apic/`, `interrupt/`, `asm/`, `include/` |
| [../src/kernel/mm/](../src/kernel/mm) | Physical page allocation, page-table management, user-copy validation, and boot-critical mapping/reservation helpers. | `page_frame.*`, `virtual_memory.*`, `user_copy.*`, `boot_mapping.*`, `boot_reserve.*` |
| [../src/kernel/proc/](../src/kernel/proc) | Process and thread object lifecycle, process table ownership, thread frame setup, deferred reaping, and initrd-backed user-program loading. | `process.*`, `thread.*`, `thread_layout.inc`, `reaper.cpp`, `user_program.*` |
| [../src/kernel/sched/](../src/kernel/sched) | Scheduling policy, runnable selection, scheduler handoff, and idle-thread behavior. | `scheduler.*`, `thread_queue.cpp`, `idle.*` |
| [../src/kernel/syscall/](../src/kernel/syscall) | User/kernel ABI numbers, register-level dispatch, and individual syscall bodies. | `abi.hpp`, `dispatch.hpp`, `process.hpp`, `console_read.hpp`, `wait.hpp`, `observe.hpp`; syscall numbers live in [../src/uapi/os1/syscall_numbers.h](../src/uapi/os1/syscall_numbers.h) |
| [../src/kernel/console/](../src/kernel/console) | Logical terminals, console byte streams, serial/keyboard line input, and terminal switching policy. | `terminal.*`, `console.*`, `console_input.*`, `terminal_switcher.*`, `pty/README.md` |
| [../src/kernel/drivers/](../src/kernel/drivers) | Device-specific hardware drivers. Driver folders must not own platform-wide discovery policy. | `block/virtio_blk.*`, `display/text_display.*`, `input/ps2_keyboard.*`, `timer/pit.*`, future `bus/`, `net/`, `virtio/` notes |
| [../src/kernel/platform/](../src/kernel/platform) | ACPI/PCI machine discovery, normalized platform state, CPU/APIC topology publication, legacy MP fallback, ISA IRQ routing, and device-probe sequencing. | `platform.hpp`, `types.hpp`, `state.hpp`, `init.cpp`, `acpi.hpp`, `pci.hpp`, `topology.hpp`, `irq_routing.hpp`, `legacy_mp.hpp`, `device_probe.hpp` |
| [../src/kernel/storage/](../src/kernel/storage) | Generic storage abstractions above concrete block drivers. | `block_device.hpp`, `README.md` |
| [../src/kernel/fs/](../src/kernel/fs) | Concrete filesystem/archive parsers. Today this is the boot initrd CPIO parser, not a general namespace. | `initrd.*` |
| [../src/kernel/vfs/](../src/kernel/vfs) | Future filesystem namespace layer: mount table, path lookup, file descriptors, and filesystem-backed `exec`. | `README.md` |
| [../src/kernel/security/](../src/kernel/security) | Future credentials, permissions, and resource-boundary policy. | `README.md` |
| [../src/kernel/debug/](../src/kernel/debug) | Serial debug logger and future tracing/counter surfaces. | `debug.*` |
| [../src/kernel/util/](../src/kernel/util) | Small generic helpers with no subsystem ownership. | `assert.hpp`, `align.hpp`, `ctype.hpp`, `fixed_string.hpp`, `memory.h`, `string.*` |
| [../src/kernel/linker/](../src/kernel/linker) | Kernel linker scripts and link-layout variants. | `kernel_bios.ld`, `kernel_limine.ld` |

The split is intentionally monolithic at link time. These folders express ownership and readability, not a module ABI or loadable-driver boundary.

## System Diagram

The diagram below is the end-to-end picture of a running `os1` system: the two boot frontends, the shared `BootInfo` contract, the kernel subsystems that come up on top of it, and the user-facing shell plus observability UAPI. Solid arrows are runtime data/control flow; dashed arrows mark per-boot one-shot handoffs.

```text
+---------------------------+                           +---------------------------+
|  QEMU q35  (BIOS path)    |                           |  QEMU q35  (UEFI path)    |
|  raw image  os1.raw       |                           |  OVMF  +  os1.iso         |
+-------------+-------------+                           +-------------+-------------+
              |                                                       |
              v                                                       v
     +--------+---------+                                    +--------+---------+
     | boot.bin (MBR)   |                                    | Limine           |
     | kernel16.bin     |                                    | (BOOTX64.EFI)    |
     | 16->long mode    |                                    +--------+---------+
     | E820, EDD, RSDP  |                                             |
     +--------+---------+                                             v
              |                                         +-------------+-------------+
              |                                         | kernel_limine.elf         |
              |                                         | (higher-half shim)        |
              |                                         | HHDM, memmap, fb, RSDP,   |
              |                                         | SMBIOS, modules, cmdline  |
              |                                         | virt->phys translate,     |
              |                                         | load kernel.elf,          |
              |                                         | install low identity win  |
              |                                         +-------------+-------------+
              |                                                       |
              |  populates BootInfo @ 0x500                            |  mirrors layout
              |  + memmap @ 0x6000                                     |  builds BootInfo
              |  + modules @ 0x7000                                    |  in same arena
              |  + strings @ 0x7200                                    |
              +---------------------------+   +-------------------------+
                                          v   v
                              +-------------+----------------+
                              |  BootInfo (packed, versioned)|
                              |  source, magic, rsdp, fb,    |
                              |  memory_map[], modules[]     |
                              +--------------+---------------+
                                             |
                             kernel_main(BootInfo*, cpu*)  <-- shared entry
                                             |
                                             v
+-----------------------------------------------------------------------------+
|                         kernel.elf        (shared core)                     |
|                                                                             |
|  own_boot_info --> PFA(bitmap) --> kernel page tables --> CR3 switch          |
|                                                                             |
|  platform_init:   ACPI discovery --> PCIe ECAM enumeration                   |
|                   | CPUs, IOAPIC, LAPIC, IRQ overrides                      |
|                   | BAR sizing, virtio-blk probe                            |
|                   v BlockDevice facade + sector 0/1 smoke                   |
|                                                                             |
|  PIC+IOAPIC+LAPIC  -->  cpu_bootothers  -->  APs in cpu_idle_loop (cli;hlt) |
|                                                                             |
|  Terminals[12]  -->  Display backend: VgaText | FramebufferText | serial    |
|  console_input:  PS/2 scancodes + serial RX ---> pending line buffer        |
|                                                                             |
|  IDT + exception handlers + IRQ0 (timer 1000 Hz) + IRQ1 (keyboard)          |
|                                                                             |
|  Task tables (Process, Thread) + idle thread + round-robin scheduler        |
|                                                                             |
|  initrd (cpio newc) --> ELF64 load /bin/init --> exec /bin/sh               |
+--------------------------------+----------------+---------------------------+
                                 |                |
                      int 0x80   |                |   reads from console_input
                      (vector 48)|                |   and observe buffers
                                 v                v
                         +-------+----------------+-------+
                         |  Ring-3 user processes          |
                         |                                 |
                         |  /bin/init -> /bin/sh           |
                         |  /bin/yield  /bin/fault         |
                         |  /bin/copycheck  /bin/ascii    |
                         |                                 |
                         |  syscalls:  write, read, exit,  |
                         |    yield, getpid, observe,      |
                         |    spawn, waitpid, exec         |
                         |                                 |
                         |  UAPI: os1/observe.h (versioned |
                         |    fixed-record snapshots)      |
                         +---------------------------------+
```

Key invariants visible in the diagram:

- Two boot frontends, one `BootInfo` at a well-known low-memory address, one `kernel_main` entry.
- The kernel never reads Limine-virtual pointers; everything is translated to physical before the kernel sees it.
- ACPI drives both CPU topology (MADT) and PCIe windows (MCFG). Legacy MP tables are a BIOS-only fallback.
- User space reaches the kernel through a single interrupt gate (vector 48) and reads kernel state through one UAPI header.

## Boot And Runtime Workflow

The lifecycle below is the exact sequence the system follows today. Each phase is phrased in the tense it runs in so the document reads like a timeline rather than a wish list.

### Phase 1 — firmware and frontend (one-shot)

- **BIOS path:** BIOS loads `boot.bin` at `0x7C00`. The MBR chain-loads `kernel16.bin`, which enables A20, probes long-mode support, reads the kernel and initrd through BIOS EDD, captures the text cursor, collects E820 memory regions, scans standard BIOS ranges for the ACPI RSDP, builds temporary page tables, enables `LME`/`NXE`, and jumps to 64-bit code.
- **UEFI path:** OVMF loads Limine. Limine parses `limine.conf`, loads `kernel_limine.elf` as the executable and publishes `kernel.elf` + `initrd.cpio` as modules. The shim's `_start` switches to its own 16 KiB stack and calls `limine_start_main`.

Both paths finish this phase holding (or able to reach) every piece of bootloader-native data they need for the next step.

### Phase 2 — `BootInfo` build (one-shot, mirrored layout)

On UEFI, `BuildBootInfo()` walks Limine's HHDM to read memmap entries, translates every Limine-virtual pointer (framebuffer, RSDP, SMBIOS, initrd) into physical addresses with `TranslateLimineVirtual()`, and writes the result into a `LowHandoffBootInfoStorage` block placed immediately after the loaded kernel image. On BIOS the 64-bit loader writes the same layout from E820 + EDD + CMOS + the earlier RSDP scan.

Both paths finalize identical low-memory arenas:

- `0x0500` — `BootInfo` header
- `0x6000` — `BootMemoryRegion[]` memory map
- `0x7000` — `BootModuleInfo[]` module descriptors
- `0x7200` — string pool (bootloader name, command line, module names)

The shim then installs a minimum low-identity window (PML4[0] → PML3[0] → 2 MiB PML2 pages covering only the handoff region) and calls `limine_enter_kernel`, which switches to the per-CPU boot stack and jumps to `kernel_main(BootInfo*, cpu*)`.

### Phase 3 — kernel bring-up (`kernel_main`)

The shared entry runs one deterministic sequence:

1. `debug("[kernel64] hello!")` on serial.
2. `own_boot_info()` deep-copies the header, memory map, modules, and all strings into kernel BSS. After this line, bootloader staging memory is no longer referenced.
3. Boot CPU page (`cpu_boot`) is templated, `cpu_init()` loads the GDT, TSS, kernel CR3, and gs base.
4. `PageFrameContainer` is initialized from `std::span<const BootMemoryRegion>`: mark all pages busy → free only `Usable` regions → reserve the bitmap → reserve low bootstrap → reserve the kernel image.
5. `reserve_tracked_physical_range` reserves every initrd module and the framebuffer.
6. Kernel identity page tables are built for all usable RAM above `kKernelReservedPhysicalStart`, then modules, framebuffer, and the RSDP page are explicitly `map_identity_range`d because they may live in non-usable memory on the modern path.
7. `kvm.activate()` switches CR3 to the kernel root; `g_kernel_root_cr3` records it.
8. `platform_init(*g_boot_info, kvm)` runs the ACPI → PCIe → virtio-blk pipeline (see Phase 4).
9. `pic_init`, `ioapic_init`, `lapic_init`, `cpu_bootothers(g_kernel_root_cr3)` bring up the 8259 in masked mode, program the IOAPIC, activate the LAPIC, and start APs. APs land in `cpu_idle_loop()` (`cli; hlt`).
10. 12 terminals are allocated (one page each), the display backend is selected from `BootInfo.source` + framebuffer pixel format, and `active_terminal` prints `[kernel64] hello`.
11. IDT initialization registers every exception handler with `on_kernel_exception`, the keyboard and console-input subsystems are initialized, and ISA IRQs for timer and keyboard are routed through the IOAPIC when SMP is active.
12. Task tables (`Process[32]`, `Thread[32]`) are allocated from page frames, a kernel process is created, and a kernel idle thread is created.
13. `LoadUserProgram("/bin/init")` loads the first user program from the initrd (`ET_EXEC`, `PT_LOAD` segments only, SysV-shaped initial stack). `/bin/init` then replaces itself with `/bin/sh` through `exec`.
14. `set_timer(1000)` sets the 1000 Hz PIT tick, `start_multi_task(init_thread)` enters the first user process via `iretq`.

### Phase 4 — ACPI, PCIe, and storage probe (inside `platform_init`)

```text
BootInfo.rsdp_physical
        |
        v
 ResolveAcpiTables   ---> XSDT (preferred) or RSDT (fallback)
        |
        |-- walk root table entries
        |     find APIC (MADT) and MCFG signatures
        v
 ParseMadt   ---> CpuInfo[], IoApicInfo[], InterruptOverride[]
 ParseMcfg   ---> PciEcamRegion[]
        |
        v
 AllocateCpusFromTopology  ---> cpu alloc, LAPIC/IOAPIC mapping
        |
        v
 EnumeratePci  ---> ECAM sweep, header type, multi-function,
        |           BAR sizing with command register IO-disable
        |           window
        v
 ProbeVirtioBlkDevice  ---> vendor-specific cap walk,
        |                   common_cfg/notify_cfg/device_cfg,
        |                   feature negotiation (VERSION_1),
        |                   3-page queue (desc/avail/used),
        |                   request scratch page
        v
 BlockDevice(read + stub write) ---> read sector 0, verify prefix
        |                          read sector 1, verify prefix
        v                          -> "virtio-blk smoke ok"
 RunVirtioBlkSmoke
```

On Limine, missing or malformed ACPI tables are a hard error. On BIOS, the legacy MP-table fallback (`UseLegacyMpFallback`) is still accepted to keep the compatibility path viable.

### Phase 5 — operator-visible runtime

```text
 PIT IRQ0 (1000 Hz)         PS/2 IRQ1            serial RX (polled)
        |                        |                      |
        v                        v                      v
 HandleIrq            +---- Keyboard::Poll      ConsoleInputPollSerial
   |   AckLegacy      |          |                      |
   |   tick++         |          +----->  console_input pending line
   |   poll serial    |                       |
   |                  |                       v
   v                  |                enter key wakes read(0, ...)
 ScheduleNext(true)  <+
        |
        v
 Thread::frame ---iretq---> ring 3 shell
```

The shell runs a line-buffered REPL:

1. `write(1, "os1> ")` via `int 0x80`.
2. `read(0, buf, N)` blocks on `ThreadWaitReason::ConsoleRead` until enter commits a line.
3. Tokens are dispatched: `help`, `echo`, `pid`, built-in observers (`sys`, `ps`, `cpu`, `pci`, `initrd`) each make one `observe(kind, buf, len)` call and render the resulting fixed-record table; `exec <path>` replaces the shell image in place; unknown tokens resolve via `/bin/<name>` and are run under `spawn` + `waitpid`.
4. User exceptions (e.g. `/bin/fault`) are caught in `HandleException`, the thread is marked `Dying`, `ScheduleNext(false)` runs, and `reapDeadThreads` reclaims the thread stack and address space from a different stack on a later trap.

### Phase 6 — exec and in-place image replacement

```text
 SysExec(user_path)
        |
        v
 LoadUserProgramImage(path)  --> new_cr3, entry, user_rsp
        |
        v
 thread->address_space_cr3 = new_cr3
 thread->process->address_space.cr3 = new_cr3
 PrepareUserThreadEntry(thread, entry, user_rsp)
 WriteCr3(new_cr3)
        |
        v
 DestroyUserAddressSpace(old_cr3)
        |
        v
 (return to user via iretq with new frame)
```

The `exec` smoke enforces that control never returns to the *old* shell: the rejected-marker list includes `shell exec returned` and an `echo after-exec` probe. This is how the tests turn "no return" into a mechanical guarantee.

## Build Outputs And Boot Artifacts

The CMake build now produces two image families:

- `build/artifacts/os1.iso`
  This is the default artifact. It is a UEFI-only Limine ISO.
- `build/artifacts/os1.raw`
  This is the explicit legacy BIOS compatibility image.

It also produces the boot payloads that feed those images:

- `boot.bin`
- `kernel16.bin`
- `kernel.elf`
- `kernel_limine.elf`
- `initrd.cpio`
- `virtio-test-disk.raw`
- `user/init.elf`, `user/sh.elf`, `user/yield.elf`, `user/fault.elf`, `user/copycheck.elf`, `user/ascii.elf`

The ISO stages these files:

- `EFI/BOOT/BOOTX64.EFI`
- `limine.conf`
- `boot/limine/limine-uefi-cd.bin`
- `kernel_limine.elf`
- `kernel.elf`
- `initrd.cpio`

The BIOS raw image keeps a fixed LBA layout generated at configure time and emitted into NASM through `cmake/templates/image_layout.inc.in`:

- LBA `0`: MBR boot sector
- LBA `1-64`: `kernel16.bin`
- LBA `65-832`: `kernel.elf`
- LBA `833-960`: `initrd.cpio`

The slot sizes come from `OS1_LOADER16_IMAGE_SECTOR_COUNT`, `OS1_KERNEL_IMAGE_SECTOR_COUNT`, and `OS1_INITRD_IMAGE_SECTOR_COUNT` in [../CMakeLists.txt](../CMakeLists.txt). The kernel BIOS slot now matches the reserved low-physical kernel window published through the shared memory-layout contract. The build runs an ELF-aware boot-envelope assertion before writing `os1.raw` and fails if `kernel.elf` no longer fits its disk slot, low-memory staging buffer, or reserved execution window; `initrd.cpio` is still checked against its configured slot. To expand BIOS storage space, increase the corresponding sector-count or reserved-window value in [../CMakeLists.txt](../CMakeLists.txt) and rebuild; the generated BIOS image layout updates automatically while `os1.raw` stays padded to 1 MiB.

## The Shared Kernel Contract: `BootInfo`

[../src/kernel/handoff/boot_info.hpp](../src/kernel/handoff/boot_info.hpp) defines the only boot contract the kernel consumes.

Important properties:

- versioned and magic-checked
- bootloader-agnostic
- packed and layout-asserted so C++ and NASM stay in sync
- rich enough to cover both current and future boot paths

Fields include:

- boot source enum: `BiosLegacy`, `Limine`, `TestHarness`
- kernel physical start and end
- optional firmware pointers such as `rsdp_physical` and `smbios_physical`
- optional strings for bootloader name and command line
- text-console metadata
- framebuffer metadata
- memory map pointer plus count
- module pointer plus count

`BootFramebufferInfo.pixel_format` is a typed enum, not an ad-hoc integer. The current kernel recognizes:

- `Unknown`
- `Rgb`
- `Bgr`

The kernel immediately calls `own_boot_info()` on entry. That function deep-copies:

- the header
- the memory map
- the module list
- bootloader name
- command line
- per-module names

into kernel-owned BSS storage. That matters because bootloader staging memory is not a stable ownership boundary. Once `own_boot_info()` returns, the rest of the kernel no longer depends on bootloader-owned metadata buffers.

## Early Physical Layout Shared By Boot Paths

[../src/kernel/handoff/memory_layout.h](../src/kernel/handoff/memory_layout.h) and [../src/kernel/handoff/memory_layout.inc](../src/kernel/handoff/memory_layout.inc) hold the fixed early-boot addresses used by the BIOS loader and mirrored by the Limine shim.

Key addresses:

| Address | Purpose |
| --- | --- |
| `0x0500` | `BootInfo` header |
| `0x6000` | `BootMemoryRegion[]` buffer |
| `0x7000` | `BootModuleInfo[]` buffer |
| `0x7200` | low-memory string pool for bootloader name, cmdline, module names |
| `0x0A000` | temporary page-table scratch used by the BIOS long-mode transition |
| `0x10000` | BIOS kernel-image load buffer |
| `0x80000` | BIOS initrd load buffer |
| `0x100000` | low-half kernel link/load base |
| `0x20000-0x5FFFF` | page-frame bitmap |

The modern path deliberately mirrors this layout before entering `kernel_main`. That is not nostalgia. It is a compatibility technique that keeps the shared kernel core bootloader-agnostic while the project still uses a low identity-linked kernel.

## Modern Default Boot Path: Limine + UEFI

### Firmware And Bootloader Flow

The default run path is:

1. QEMU starts `q35` with OVMF.
2. OVMF loads Limine from `EFI/BOOT/BOOTX64.EFI`.
3. Limine reads `limine.conf`.
4. Limine loads `kernel_limine.elf` as the executable and publishes `kernel.elf` plus `initrd.cpio` as modules.
5. Control enters `_start()` in `src/boot/limine/entry.cpp`.

The Limine config currently requests a `1024x768x32` framebuffer and enables serial output so boot can always be verified through logs.

### Limine Requests

The frontend requests and consumes:

- HHDM
- memory map
- framebuffer
- bootloader info
- executable command line
- modules
- ACPI RSDP
- SMBIOS

Those native Limine structures do not escape into the kernel. The shim normalizes them into `BootInfo`.

### Why The Shim Exists

The kernel core remains low-linked and assumes that physical addresses are valid supervisor virtual addresses once the early identity window exists. Limine, however, boots the executable frontend in a modern higher-half environment.

The shim resolves that mismatch in three steps:

1. It uses the Limine HHDM mapping to access physical memory safely.
2. It loads the shared low-half kernel image by physical address.
3. It patches the active page tables so the shared kernel can start through low identity addresses.

That means the kernel core sees the same ABI and roughly the same addressing model whether it was entered from BIOS or from UEFI.

### Virtual-To-Physical Normalization

Limine gives the shim several pointers that are valid in the bootloader's current virtual address space:

- module addresses
- framebuffer address
- RSDP pointer
- SMBIOS pointers

The shim does not pass those virtual addresses through to the kernel. Instead, `TranslateLimineVirtual()` walks the currently active page tables by:

- reading `CR3`
- following PML4/PML3/PML2/PML1 entries through HHDM
- extracting the backing physical address

That translation step is central to Milestone 3. It ensures `BootInfo` remains a physical-address contract rather than a Limine-virtual-address contract.

### Loading The Shared Low-Half Kernel

The shared kernel image is `kernel.elf`, exposed by Limine as a module. The shim parses its ELF64 program headers and handles `PT_LOAD` segments only.

Important implementation detail:

- the shim writes segment contents through the HHDM mapping
- it does not assume the low physical target range is already identity-mapped

This is why the modern path now works reliably. The earlier assumption that Limine would leave the entire low boot-critical window identity-mapped was false.

The shim also allocates the initial boot CPU page immediately after the loaded kernel image, matching the BIOS path's contract.

### Installing The Low Identity Window

After loading the low-half kernel, the shim calls `EnsureLowIdentityWindow()`.

This function patches the active page tables so the first boot-critical physical window becomes executable and writable through identity addresses. It installs only the minimum mapping needed for handoff by creating or filling:

- PML4 slot `0`
- PML3 slot `0`
- PML2 2 MiB large-page entries

The goal is not to keep Limine's page tables forever. The goal is to make the final jump into `kernel_main` valid while preserving the rest of Limine's higher-half environment long enough for the kernel to copy `BootInfo` and build its own page tables.

### Building The Final `BootInfo`

The shim then mirrors the BIOS handoff layout into low memory:

- `BootInfo` at `0x0500`
- memory map at `0x6000`
- module descriptors at `0x7000`
- strings at `0x7200`

That low-memory block is populated with:

- `source = Limine`
- bootloader name and version
- command line when present
- normalized memory map
- normalized initrd module physical range
- framebuffer physical address and geometry
- translated ACPI and SMBIOS physical pointers
- kernel physical start/end

The final transfer happens through `limine_enter_kernel()`, which:

- switches to the low boot CPU page as the stack
- passes `RDI = BootInfo*`
- passes `RSI = cpu*`
- calls the shared low-half `kernel_main`

## Legacy BIOS Compatibility Path

The BIOS path remains in tree and is continuously tested.

Flow:

1. BIOS loads `boot.bin` at `0x7C00`.
2. The MBR stage loads the first sector of `kernel16.bin`.
3. `kernel16.bin` finishes loading itself through CHS reads.
4. The loader enables A20, checks long-mode support, reads the kernel and initrd through EDD packet reads, captures the BIOS cursor, collects the E820 memory map, and scans the standard BIOS ACPI search ranges for a valid RSDP.
5. `src/boot/bios/long64.asm` builds temporary page tables, enables `LME` and `NXE`, and jumps into long mode.
6. The 64-bit loader expands `kernel.elf` at `0x00100000`, builds `BootInfo`, allocates the boot CPU page, and calls `kernel_main`.

The BIOS path still exists for three reasons:

- it provides a low-complexity fallback path
- it preserves bring-up knowledge
- it is now cheap to keep because the kernel contract is shared

It is no longer the default workflow.

## Kernel Initialization After Handoff

`kernel_main()` in [../src/kernel/core/kernel_main.cpp](../src/kernel/core/kernel_main.cpp) is the shared kernel entry for both paths. It is now high-level boot orchestration only: trap routing lives in `core/trap_dispatch.cpp`, IRQ flow in `core/irq_dispatch.cpp`, fault policy in `core/fault.cpp`, syscall dispatch in `syscall/dispatch.cpp`, scheduler handoff in `sched/scheduler.cpp`, platform sequencing in `platform/init.cpp`, and device-specific behavior under `drivers/`.

The high-level sequence is:

1. serial hello
2. `own_boot_info()`
3. boot CPU initialization
4. physical page-frame allocator initialization
5. reserve boot modules and framebuffer ranges
6. build kernel-owned identity-mapped page tables
7. map boot-critical non-usable ranges such as initrd, framebuffer, and the initial RSDP page
8. switch to the kernel's `CR3`
9. run `platform_init()` to parse ACPI, normalize topology, enumerate PCIe, and probe `virtio-blk`
10. initialize PIC, IOAPIC, LAPIC
11. start APs when available
12. allocate terminals
13. select display backend
14. initialize interrupts and keyboard
15. initialize scheduler tables
16. create kernel idle thread
17. load user programs from initrd
18. start multitasking

The important architectural point is that the boot frontends stop mattering almost immediately after `own_boot_info()`. After that point, the system is running on kernel-owned page tables and kernel-owned copies of boot metadata.

## Console And Display Architecture

Milestone 3 introduced a real split between the logical terminal model and the physical presentation backend.

### Terminal Model

[../src/kernel/console/terminal.cpp](../src/kernel/console/terminal.cpp) owns the text-cell model:

- geometry chosen at boot from the active display backend
- VGA stays 80x25; the framebuffer path derives columns and rows from framebuffer size and 8x16 cells
- cursor tracking
- multiple terminal buffers
- write/scroll/clear behavior

The terminal remains fixed-size for the lifetime of a boot, but not across all boot paths. BIOS uses the traditional 80x25 VGA geometry. The framebuffer path instead sizes the terminal from the discovered framebuffer dimensions, for example `1024x768` becomes `128x48` text cells.

### Console Input Path

[../src/kernel/console/console_input.cpp](../src/kernel/console/console_input.cpp) owns the shared canonical input path used by the shell.

- decoded keyboard input and serial RX feed the same pending line buffer
- backspace edits the pending line locally before it is committed
- enter commits a complete line and wakes a blocking `read(0, ...)`
- serial echo stays enabled so local scripted runs and CI smoke transcripts exercise the same prompt/command flow

This keeps manual keyboard sessions and serial-driven automation on one kernel-visible console path instead of two separate control planes.

### Display Backends

[../src/kernel/drivers/display/text_display.cpp](../src/kernel/drivers/display/text_display.cpp) adds two presentation backends:

- `VgaTextDisplay`
- `FramebufferTextDisplay`

The active backend is chosen from `BootInfo`:

- BIOS defaults to VGA text mode
- Limine/UEFI uses the framebuffer backend when the format is supported
- otherwise the kernel continues with serial-only diagnostics

### Framebuffer Text Rendering

The framebuffer backend is intentionally minimal:

- text grid derived from framebuffer geometry using 8x16 cells
- bundled `font8x16` bitmap glyphs
- white text on black background
- software cursor by inverse cell rendering
- cell-by-cell redraw only where content or cursor state changed

This is not a full graphics subsystem. It is a presentation adapter for the existing terminal model.

The framebuffer renderer maintains a shadow buffer and skips unchanged rows and cells. Serial remains the authoritative debug channel.

## Memory Architecture

### Physical Page Allocation

`PageFrameContainer` in [../src/kernel/mm/page_frame.cpp](../src/kernel/mm/page_frame.cpp) owns the physical page allocator. It is bitmap-based and now seeds itself directly from `std::span<const BootMemoryRegion>` rather than an older BIOS-specific structure.

The allocator:

- marks all pages busy by default
- frees only `BootMemoryType::Usable` ranges
- reserves the bitmap itself
- reserves low bootstrap ranges
- reserves the kernel image reservation
- can reserve arbitrary physical ranges through `ReserveRange()`

That last capability matters on the modern path because initrd modules and framebuffers may live in memory-map regions that are not simple "usable RAM" entries.

### Virtual Memory

`VirtualMemory` in [../src/kernel/mm/virtual_memory.cpp](../src/kernel/mm/virtual_memory.cpp) manages kernel and user mappings. The kernel still uses low identity mappings, but user mappings now live in a dedicated PML4 slot:

- `kUserPml4Index = 1`
- `kUserSpaceBase = 0x0000008000000000`
- `kUserImageBase = 0x0000008000400000`
- `kUserStackTop = 0x0000008040000000`

The dedicated user slot exists because the kernel still treats physical addresses as valid supervisor virtual addresses. Separating user mappings into another PML4 slot avoids colliding with that legacy-but-useful identity-mapped kernel model.

Supported operations include:

- explicit physical mappings with `PageFlags`
- page allocation plus mapping
- protection updates
- virtual-to-physical translation
- cloning the kernel PML4 entry into a process address space
- destroying the user slot during process teardown

## CPU, Interrupt, And Trap Architecture

### CPU State

Each CPU gets a dedicated `struct cpu` page containing:

- self-pointer
- current thread pointer
- per-CPU GDT
- TSS
- per-CPU trap storage
- kernel stack

The BSP receives its initial `cpu` page from the boot frontend. Additional CPUs are allocated by the kernel.

### GDT And TSS

The GDT now includes:

- kernel code/data segments
- user code/data segments
- 64-bit TSS descriptor

The TSS is used for `RSP0`, not for hardware task switching. Every context switch updates `cpu_cur()->tss.rsp0` so the next ring-3 entry will return to the correct kernel stack.

### Unified `TrapFrame`

The assembly trap entry code now builds a shared `TrapFrame` layout for:

- exceptions
- IRQs
- syscalls
- scheduler return paths

That means the C++ dispatch logic receives one coherent view of machine state, and the scheduler can return to either kernel or user contexts through the same `iretq`-based mechanism.

### Syscalls

The current syscall ABI uses `int 0x80` on vector `48`, configured as a user-callable interrupt gate.

Current syscalls:

- `write`
- `read`
- `observe`
- `spawn`
- `waitpid`
- `exec`
- `exit`
- `yield`
- `getpid`

This ABI is still intentionally small. It now covers console I/O, observability, and initrd-backed process control, but it is not yet a general POSIX-like descriptor model.

### Observability ABI

The shell does not parse serial boot logs to inspect kernel state. Instead, `observe(kind, buffer, size)` copies versioned fixed-record snapshots described by [`src/uapi/os1/observe.h`](../src/uapi/os1/observe.h) out of the kernel.

Current observe kinds include:

- system summary
- process / thread table snapshot
- discovered CPUs
- enumerated PCI devices
- packaged initrd files

The ring-3 shell consumes those records through built-ins such as `sys`, `ps`, `cpu`, `pci`, and `initrd`, which keeps the user-facing observability contract explicit.

## Process And Userland Architecture

### Scheduler Model

The old `Task` model has been replaced by:

- `AddressSpace`
- `Process`
- `Thread`

Milestone 2 still keeps one thread per process, but the scheduler runs `Thread` objects, not processes. The tables are fixed-capacity in this milestone and are allocated from page frames rather than from hard-coded low memory.

Scheduling is simple round-robin. The BSP also has a kernel idle thread. When all user work is gone, the system falls back to that idle thread instead of panicking.

### User Address Spaces

Each user process gets:

- its own PML4 root
- kernel mappings cloned from the shared kernel root
- user ELF segments mapped in user slot `1`
- a 64 KiB user stack with a guard page below it

The kernel validates user pointers through `mm/user_copy.cpp` before copying syscall payloads. That boundary rejects null, non-canonical, overflowing, and out-of-user-range addresses. It also requires user-accessible mappings for reads and user-writable mappings for kernel-to-user writes. The kernel does not rely on deliberate kernel faults as a normal syscall-copy mechanism.

### Initrd And ELF Loader

The initrd is a `cpio newc` archive built from `src/user`.

Current user programs:

- `/bin/init` — minimal init process that `exec`s `/bin/sh`
- `/bin/sh` — the ring-3 shell built from [`src/user/programs/sh.cpp`](../src/user/programs/sh.cpp)
- `/bin/yield` — cooperative yield probe built from [`src/user/programs/yield.cpp`](../src/user/programs/yield.cpp)
- `/bin/fault` — deliberate page-fault probe built from [`src/user/programs/fault.cpp`](../src/user/programs/fault.cpp)
- `/bin/copycheck` — negative syscall-copy regression probe built from [`src/user/programs/copycheck.cpp`](../src/user/programs/copycheck.cpp)
- `/bin/ascii` — ASCII table probe built from [`src/user/programs/ascii.cpp`](../src/user/programs/ascii.cpp) to visually verify 8x16 font rendering on the framebuffer text backend

The kernel keeps its fixed `/bin/init` boot contract, but init is now a real first user process rather than an alias for the shell. It immediately calls `exec("/bin/sh")`, so later init responsibilities can grow without changing the kernel boot path. The kernel parses the initrd, finds those paths, and loads ELF64 `ET_EXEC` images with `PT_LOAD` segments only. It maps segment permissions from ELF flags and zero-fills `memsz - filesz` for `.bss`.

This userland is still intentionally a vertical slice, not a general Unix process-launch environment. The kernel can boot the shell, `spawn(path)` a child initrd program for foreground commands, `waitpid(pid)` for child completion, and `exec(path)` to replace the current image in place. There is still no filesystem-backed `exec`, no `fork`, and no arguments/environment yet.

### Process Fault Handling And Teardown

If a user process faults:

- the kernel detects ring 3 from the saved code segment
- logs the fault
- marks the thread and process dying
- schedules away
- reaps the dead process later from another stack

This deferred teardown is important. The kernel does not free the current thread's stack or address space while it is still executing on it.

## Modern Platform Support

Milestone 4 replaces legacy MP-table discovery as the primary platform path.

The kernel now:

- consumes `BootInfo.rsdp_physical` on both Limine and BIOS boots
- parses `XSDT` first and falls back to `RSDT` when needed
- uses `MADT` as the primary source of CPU, LAPIC, IOAPIC, and IRQ-override topology
- uses `MCFG` to discover PCIe ECAM ranges
- enumerates PCIe devices and records BAR information
- probes a modern `virtio-blk` PCI device, publishes it through a minimal `BlockDevice` facade with implemented reads and stubbed writes, and validates raw sector reads during boot

On the default `q35` targets, both boot paths now discover four CPUs from ACPI and successfully bring up the APs.

APs still enter `cpu_idle_loop()`, an explicit interrupt-disabled `cli; hlt` loop, after their local initialization. Full multi-CPU user scheduling is still a later milestone.

## Test And CI Architecture

### Local Targets

The main CMake targets are:

- `os1_image`
- `os1_bios_image`
- `run`
- `run_serial`
- `run_bios`
- `run_bios_serial`
- `smoke`
- `smoke_observe`
- `smoke_spawn`
- `smoke_exec`
- `smoke_bios`
- `smoke_observe_bios`
- `smoke_spawn_bios`
- `smoke_exec_bios`
- `smoke_all`

`run` boots the default UEFI ISO under OVMF on `q35` and attaches the generated `virtio-blk` test disk. `run_serial` uses the same guest image but attaches the shell to serial stdio in the terminal. `run_bios` boots the raw image under BIOS on `q35` with the same secondary `virtio-blk` test disk attached, while `run_bios_serial` keeps that boot path but routes the guest shell through serial stdio.

### Smoke Tests

`CTest` now registers an eight-test shell matrix:

- `os1_smoke`
- `os1_smoke_observe`
- `os1_smoke_spawn`
- `os1_smoke_exec`
- `os1_smoke_bios`
- `os1_smoke_observe_bios`
- `os1_smoke_spawn_bios`
- `os1_smoke_exec_bios`

The baseline smoke tests cover the common boot and shell transcript on each frontend, including:

- boot-source identification and stable prompt reachability
- ACPI `MADT` topology discovery
- ACPI `MCFG` discovery
- PCIe enumeration and successful `virtio-blk` sector reads
- initrd discovery and first user-process startup
- stable shell built-ins such as `help`, `echo`, and `pid`

The dedicated observe, spawn, and exec smokes then exercise the operator-facing behavior that Milestone 5 added:

- structured `sys` / `ps` / `cpu` / `pci` / `initrd` output
- child-process launch, user-fault containment, and prompt recovery
- in-place `exec` replacement without the old shell prompt returning

### CI

GitHub Actions runs on `ubuntu-24.04` and does all of the following on every push and pull request:

- install host tools including `cpio`, `xorriso`, `ovmf`, `qemu-system-x86`
- install the `x86_64-elf` cross toolchain through Homebrew
- configure the project
- build the default modern artifact
- explicitly build the BIOS compatibility artifact
- run the full eight-test shell smoke matrix through `ctest`

The same single CI job name is kept for local `act` compatibility.

## Current Constraints And Next Step

The current architecture is coherent, but intentionally incomplete.

Major constraints that remain:

- the shared kernel core is still low identity-linked rather than higher-half
- the framebuffer path is a text presenter, not a graphics stack
- userland is still initrd-backed and single-user rather than filesystem-backed and multiuser
- syscalls still use `int 0x80`, not `SYSCALL`/`SYSRET`
- the first `virtio-blk` path is polling-only and smoke-oriented, with only a minimal block-device facade rather than a general block layer
- MSI / MSI-X, `virtio-net`, NVMe, and real filesystem-backed storage are still follow-on work

The next major work is therefore not another boot or shell bring-up refactor. It is storage, networking, and richer filesystem-backed userland on top of the current platform and operator shell base:

- block-device growth beyond the current polling `virtio-blk` facade
- filesystem-backed loading instead of initrd-only demos
- `virtio-net` and later NIC work
- timer / interrupt refinements such as MSI / MSI-X and HPET follow-on support

At this point the architecture is intentionally in a good place for that work: the boot path is modernized, the kernel entry contract is shared, ACPI and PCIe discovery are in place, and both modern and legacy boot paths remain continuously testable on the same `q35` virtual platform.
