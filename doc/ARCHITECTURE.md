# os1 Architecture

> generated-by: Claude (Opus 4.7) · generated-at: 2026-04-22 · git-commit: `5dbab468e89aa8017bf7ba7754060896abd31839`

This document describes the current architecture of `os1` as implemented in the repository today. It covers how the system boots, how control is handed to the kernel, how memory and CPUs are organized, how user-mode programs are loaded and isolated, and how the main kernel subsystems fit together.

`os1` is now a BIOS-booted, freestanding x86_64 hobby OS with a small but real protected userspace:

- a 512-byte MBR boot sector that uses CHS to load the rest of the BIOS stage
- a 64-sector 16-bit loader that reads the kernel ELF and an initrd through EDD packet reads
- a direct real-mode to long-mode transition with NX-enabled temporary paging
- a minimal ELF64 kernel loader
- a versioned `BootInfo` handoff block shared by every future boot path
- a freestanding C++20 kernel with a few assembly stubs for traps and context switch
- VGA text output, COM1 serial debug, PS/2 keyboard input
- a bitmap physical page allocator seeded from an `E820`-derived memory map
- a 4-level page-table manager with explicit `Present/Write/User/NoExecute` flags
- legacy Intel MP-table CPU discovery (ACPI MADT is planned for M4)
- per-CPU `cpu` pages with kernel stacks, TSS, and full kernel/user GDTs
- a `Process`/`Thread` model with dynamic per-process address spaces
- an `int 0x80` syscall entry path supporting `write`, `exit`, `yield`, `getpid`
- an initrd (`cpio newc`) containing statically linked user ELFs (`/bin/init`, `/bin/yield`, `/bin/fault`)
- a unified `TrapFrame` that feeds IRQ, exception, syscall, and scheduler return paths
- a headless QEMU smoke test wired into CTest and GitHub Actions CI

This document describes the current implementation, not the target end state. Milestones 1 and 2 are complete and shape most of what follows; Milestones 3 (Limine + UEFI default boot) and 4 (ACPI/PCIe/virtio) are still on paper.

## High-Level View

```text
BIOS
  -> MBR boot sector         (src/boot/boot.asm)
  -> 16-bit loader           (src/boot/kernel16.asm, ~64 sectors)
       - A20, long-mode check
       - EDD-packet disk reads via src/boot/disk16_range.asm
       - E820 memory map via src/boot/biosmemory.asm
       - publishes versioned BootInfo at 0x0500
  -> real-mode to long-mode  (src/boot/long64.asm)
       - temporary 2 MiB identity paging at 0xA000
       - NXE + LME enabled before CR0.PG
  -> 64-bit ELF expansion    (loader_main64 in src/boot/kernel16.asm)
  -> KernelMain(BootInfo*, cpu*) in src/kernel/kernel.cpp
       -> OwnBootInfo() copies the handoff into kernel BSS
       -> boot-CPU GDT/TSS/GS setup (cpu_init)
       -> page-frame allocator seeded from memory map
       -> initrd module reserved in the bitmap
       -> MP-table CPU discovery
       -> build kernel identity address space (kvm)
       -> pic_init + ioapic_init + lapic_init
       -> AP startup via cpustart.asm trampoline
       -> allocate 12 VGA terminal backing pages
       -> IDT + exception/IRQ dispatch + int 0x80
       -> process/thread tables allocated from page frames
       -> kernel idle thread + load /bin/init, /bin/yield, /bin/fault
       -> PIT @ 1 kHz + startMultiTask -> first user process
```

## Repository Structure

| Path | Role |
| --- | --- |
| [`src/boot/`](../src/boot/) | BIOS boot sector, 16-bit loader, long-mode switch, EDD disk packets, E820, BootInfo publishing |
| [`src/kernel/`](../src/kernel/) | 64-bit kernel core: BootInfo owner, CPU/APIC, interrupts, paging, tasking, syscalls, terminals, keyboard |
| [`src/user/`](../src/user/) | First userland: `crt0`, syscall shim, `/bin/init`, `/bin/yield`, `/bin/fault`, linker script |
| [`src/libc/`](../src/libc/) | Small freestanding support routines used by the kernel |
| [`cmake/scripts/`](../cmake/scripts/) | CMake helper scripts for initrd assembly and the headless QEMU smoke test |
| [`cmake/templates/image_layout.inc.in`](../cmake/templates/image_layout.inc.in) | Generated NASM include that pins LBA ranges for boot/kernel/initrd payloads |
| [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) | CI pipeline that builds and runs the smoke test |
| [`CMakeLists.txt`](../CMakeLists.txt) | Build graph for raw disk image, user programs, initrd, run/disasm/smoke targets |

## Disk Image Layout

The raw image is still a fixed-layout BIOS disk, but the LBA ranges are now computed in [`CMakeLists.txt`](../CMakeLists.txt) and exposed to NASM via the generated [`image_layout.inc`](../cmake/templates/image_layout.inc.in). Both the boot sector and the loader use the same constants.

| LBA | Contents | Size | Produced from |
| --- | --- | --- | --- |
| 0 | MBR boot sector | 512 bytes | [`src/boot/boot.asm`](../src/boot/boot.asm) |
| 1-64 | 16-bit loader image | 64 sectors / 32 KiB | [`src/boot/kernel16.asm`](../src/boot/kernel16.asm) |
| 65-320 | Kernel ELF payload | 256 sectors / 128 KiB | `kernel64.elf` |
| 321-448 | Initrd `cpio newc` archive | 128 sectors / 64 KiB | `initrd.cpio` |

Two structural changes from the previous architecture apply:

- the 16-bit loader now occupies a generous 64-sector slot rather than the original 8-sector slot, so stage 1 has room to link in the E820, EDD range-read, BootInfo publishing, and long-mode transition routines;
- the initrd is an independent payload at its own LBA range, loaded by the same BIOS loader that reads the kernel.

There is still no partition table, filesystem, or bootable-filesystem-aware second stage. LBA ranges are contractual.

## Boot Pipeline

### 1. BIOS and MBR Stage

[`src/boot/boot.asm`](../src/boot/boot.asm) is loaded at `0x7C00`. Responsibilities:

- set real-mode segment registers and a temporary stack
- save the BIOS boot drive number from `DL`
- query geometry with INT 13h AH=08h and cache sectors-per-track + head count
- read the first sector of the 16-bit loader (LBA 1) into `0x1000` via INT 13h AH=02h CHS reads
- verify the loader signature word `0x7733`
- jump into the second stage with `DL` preserved

The reason stage 0 uses classic CHS reads (rather than EDD packets) is that some real BIOSes were observed to scribble the destination packet back over the boot sector when the EDD path was used during stage 0. CHS avoids the issue and is cheap to implement at that size.

### 2. 16-bit Loader Stage

[`src/boot/kernel16.asm`](../src/boot/kernel16.asm) is loaded at `0x1000`. Its first block of code finishes pulling the remaining 63 loader sectors off disk, again with CHS reads, before handing control to the full loader body.

Main responsibilities of the full stage:

- enable and verify the A20 line
- verify long-mode support through CPUID
- read the kernel ELF into `0x10000` and the initrd into `0x80000` via [`disk16_range.asm`](../src/boot/disk16_range.asm) EDD packet reads (chunked up to 127 sectors per INT 13h call)
- capture the BIOS cursor position through INT 10h AH=03h
- collect an `E820` memory map via [`biosmemory.asm`](../src/boot/biosmemory.asm), writing directly into the BootInfo memory-region buffer at `0x6000`
- initialise the published `BootInfo` block at `0x0500`, including the bootloader name, version, source tag (`BiosLegacy`), text-console metadata, module list (the initrd), and memory map
- allocate a temporary 16 KiB page-table area at `0xA000`
- jump into the long-mode transition routine

### 3. Temporary Long-Mode Transition

[`src/boot/long64.asm`](../src/boot/long64.asm) performs the direct real-mode to long-mode transition. Important details:

- it builds a 4-level paging structure identity-mapping the first 2 MiB
- it enables PAE/PGE in `CR4`
- it loads the temporary PML4 into `CR3`
- it sets both the `LME` and **`NXE`** bits in `EFER` before enabling paging, so subsequent kernel mappings can safely use the NX bit without triggering reserved-bit page faults
- it enables protected mode + paging in `CR0`
- it copies a tiny GDT to physical address `0x0` and performs the far jump into 64-bit mode

This stage exists only to enter 64-bit code safely. The kernel later discards it and installs its own page tables.

### 4. 64-bit Loader / ELF Expansion

Once in long mode, control moves to `loader_main64` in [`src/boot/kernel16.asm`](../src/boot/kernel16.asm).

The expansion step is intentionally simple:

- it parses the ELF header fields it needs directly from the in-memory image at `0x10000`
- it reads the first `PT_LOAD` program header
- it clears `[p_vaddr, p_vaddr + p_memsz)` and copies `filesz` bytes into place
- it zero-fills the `memsz - filesz` tail, effectively creating `.bss`

The loader assumes the kernel is linked for physical/virtual address `0x100000` (see [`src/kernel/linker2.ld`](../src/kernel/linker2.ld)) and that the kernel is contained in the first `PT_LOAD` program header. Before calling `KernelMain`, the loader:

- rounds the loaded kernel end up to the next 4 KiB boundary
- reserves one page at that address as the initial boot CPU page
- sets `RSP` to the top of that page
- passes `RDI = BootInfo*` (the published handoff at `0x0500`) and `RSI = cpu_boot` (the aligned CPU page)
- finalises BootInfo fields that only become valid post-ELF expansion (`kernel_physical_start`, `kernel_physical_end`, `bootloader_name`, module pointer, magic)

At entry, `KernelMain` sees a fully-populated and versioned BootInfo, and a pre-allocated per-CPU scratch page that becomes the first entry in the CPU linked list.

## Boot Handoff Contract

### The `BootInfo` ABI

[`src/kernel/bootinfo.h`](../src/kernel/bootinfo.h) defines the only kernel-facing boot handoff. It is versioned, POD, and independent of the bootloader's identity:

- `magic = "OS1BOOT1"` (`0x4F5331424F4F5431`)
- `version = 1`
- `source` enum: `BiosLegacy` today; `Limine` and `TestHarness` reserved
- kernel image physical bounds
- reserved slots for `rsdp_physical` and `smbios_physical` (populated when the modern boot path arrives)
- optional `command_line` and `bootloader_name` strings
- text-console metadata (columns, rows, cursor x/y)
- framebuffer metadata (present for later boot paths; zeroed on the BIOS path)
- a memory map (`BootMemoryRegion[]`) with an explicit count
- a module list (`BootModuleInfo[]`) with an explicit count; module 0 is the initrd

The structure is `#pragma pack(push, 1)`-packed and its layout is guarded by a block of `offsetof` static asserts in the header so the NASM `struc`s in [`kernel16.asm`](../src/boot/kernel16.asm) and the C++ struct stay in lockstep. The low entries of `BootMemoryType` deliberately reuse BIOS E820 numeric values so the loader can forward them without translation.

The same set of constants exists in two shapes:

- [`src/kernel/memory_layout.h`](../src/kernel/memory_layout.h) for the C++ side (`constexpr` only)
- [`src/kernel/memory_layout.inc`](../src/kernel/memory_layout.inc) for the NASM side (`%define`)

Anything a stage of boot needs to know — the BootInfo address, the initrd load address, the AP trampoline parameter layout, user-space PML4 slot index, etc. — goes through one of those two files. This is what Milestone 1 meant by "centralize low-memory constants".

### Ownership: `OwnBootInfo`

The first thing `KernelMain` does (after printing the serial hello) is call `OwnBootInfo()` in [`src/kernel/bootinfo.cpp`](../src/kernel/bootinfo.cpp). This:

- validates magic, version, and array bounds
- `memcpy`s the header into kernel BSS
- deep-copies the memory-map array (up to `kBootInfoMaxMemoryRegions = 128`)
- deep-copies the module list (up to `kBootInfoMaxModules = 16`)
- copies `bootloader_name`, `command_line`, and per-module names into fixed-size char buffers also held in kernel BSS

From that point on the kernel never touches bootloader-owned memory for BootInfo data. That is what lets later boot paths (Limine, a test harness, UEFI) reclaim their own staging regions safely, and what lets the kernel run on top of any future boot path without a special case.

### Boot Source Adapters

There is currently exactly one adapter: the BIOS loader, which writes `BootInfo` in place at the agreed low-memory address. The C++ side does not translate it; the structure is already in the canonical shape when it arrives.

The design leaves the door open for a Limine-based path (Milestone 3) and a host-side test harness that forges `BootInfo` blocks without any actual boot. Neither exists in tree today, but the kernel does not have to care which one was used — only whether the magic/version match and the pointers validate.

## Kernel Entry and Runtime Flow

The kernel entry point is `KernelMain(BootInfo*, cpu*)` in [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp#L754). Its current initialization sequence:

1. `OwnBootInfo()` deep-copies and validates the handoff into kernel BSS.
2. Copy `cpu_boot_template` into the caller-provided `cpu_boot` page and call `cpu_init()` to install the BSP's GDT, TSS, GS base, and segment registers.
3. `PageFrameContainer::Initialize()` seeds the bitmap from the BootInfo memory map and reserves early/kernel/bitmap ranges.
4. Pin initrd pages via `page_frames.ReserveRange()` so the cpio archive is not handed out for later allocations.
5. `mp_init()` walks the Intel MP floating-pointer structure and populates `ncpu`, `lapic`, `ioapic`, and allocates one `cpu` page per discovered processor.
6. Build a kernel identity address space (`VirtualMemory kvm`) that maps:
   - `[0, 0x100000)` as a single block (low bootstrap memory)
   - every `Usable` region above `0x100000` as reported by BootInfo
   - the LAPIC and IOAPIC MMIO pages
7. Activate the kernel PML4 (`kvm.Activate()`) and stash its root as `g_kernel_root_cr3` for later user-process construction.
8. `pic_init`, `ioapic_init`, `lapic_init`, then `cpu_bootothers(g_kernel_root_cr3)` to start each AP on the shared kernel address space.
9. Allocate 12 VGA terminal backing pages from the page-frame allocator, attach terminal 0 to `0xB8000`, and print the serial/VGA hello banner.
10. `Interrupts::Initialize()` builds the IDT, remaps the legacy PIC, and wires all 20 supported CPU exception vectors plus the IRQ stubs and the `int 0x80` syscall gate.
11. Register the PS/2 keyboard IRQ handler and, if SMP, route PIT and keyboard lines through the IOAPIC.
12. `initTasks()` allocates the process and thread tables from page frames (not low memory) and resets `current_thread`.
13. Build the kernel process and kernel idle thread. The idle thread loops `cli; hlt` as the "nothing-to-run" fallback and also as the BSP's terminal state once user demos finish.
14. Load three statically linked user programs from the initrd and turn each into its own `Process` + `Thread` with its own PML4.
15. Program the PIT to 1 kHz, seed `current_thread` with `/bin/init`, and call `startMultiTask()` to `iretq` into user mode.

After that call the kernel is driven entirely by interrupts: timer for preemption, `int 0x80` for syscalls, and CPU exceptions for faults.

## Memory Architecture

### Memory Model Overview

The kernel still prefers physical identity mappings for its own text/data/MMIO and for the low bootstrap region, but the `VirtualMemory` class now expresses explicit page flags (`Present`, `Write`, `User`, `NoExecute`) and knows how to build, clone, walk, and tear down user-visible tables. Each user process has its own PML4; kernel mappings are shared by cloning a specific PML4 slot.

There is still no higher-half kernel — kernel virtual addresses equal physical addresses. Milestone 2 chose to postpone a higher-half rewrite so it could land protected userspace without entangling two large refactors. User pages live in their own PML4 slot (`kUserPml4Index = 1`, base `0x0000008000000000`) so kernel physical-access mappings in slot 0 are undisturbed.

### Fixed Physical Addresses (early boot)

These are the addresses the BIOS path still depends on. They are defined in [`memory_layout.h`](../src/kernel/memory_layout.h) / [`memory_layout.inc`](../src/kernel/memory_layout.inc) rather than scattered through the source tree.

| Address / Range | Current role |
| --- | --- |
| `0x0000` | Temporary GDT placement during long-mode entry and AP startup |
| `0x0020-0x003F` | AP trampoline parameter block (CPU page, RIP, CR3, IDT ptr) |
| `0x0500` | Published `BootInfo` block (BIOS loader -> kernel) |
| `0x0800`-`0x0840` | BIOS disk packet scratch (stage 0 + stage 1 disk reads) |
| `0x1000` | 16-bit loader image; later reused as the AP trampoline page |
| `0x6000`-`0x6BFF` | BootInfo memory-region buffer (E820 target) |
| `0x7000`-`0x7017` | BootInfo module descriptor buffer (initrd descriptor) |
| `0xA000`-`0xDFFF` | Temporary real-mode page tables for entering long mode |
| `0x10000` | Low-memory kernel ELF image buffer |
| `0x20000`-`0x5FFFF` | Page-frame bitmap (256 KiB, covers up to 64 GiB) |
| `0x80000`-`0xFFFFF` | Initrd load buffer (64 KiB reserved, stays under 1 MiB for BIOS reach) |
| `0x100000`-`0x15FFFF` | Kernel code/data reservation marked busy in the page allocator |

The `0x408` task-table address and the hard-coded `0x60000` CR3 from the previous architecture are **gone**. Process, thread, and page-table storage now come from the page-frame allocator.

### User Address-Space Layout

The user ABI layout is also constant across processes today:

- `kUserPml4Index = 1` — dedicated PML4 slot used for all user pages
- `kUserSpaceBase = 0x0000008000000000` — start of the user half of the address space
- `kUserImageBase = 0x0000008000400000` — link base for user ELFs (`src/user/linker.ld`)
- `kUserStackTop = 0x0000008040000000` — top of the user stack region
- `kUserStackPages = 16` — 64 KiB user stack per thread
- One implicit guard page below the stack (the page immediately below the mapped range is left unmapped)

When a user ELF is loaded, the kernel allocates a fresh PML4 root, clones the kernel half from slot 0, then populates slot 1 with user mappings only. Segment permissions follow the ELF program-header flags: writable segments get `PageFlags::Write`, non-executable segments get `PageFlags::NoExecute`, and everything user-accessible gets `PageFlags::User`.

### Physical Page Allocation

[`src/kernel/pageframe.cpp`](../src/kernel/pageframe.cpp) implements the physical page allocator. Key properties are unchanged (4 KiB pages, one bit per page, 1 = free), but the allocator now consumes a `std::span<const BootMemoryRegion>` taken straight out of `BootInfo`, and it supports `ReserveRange(address, length)` so the kernel can pin initrd pages without knowing anything about cpio internals.

Initialization flow:

- iterate the memory map, summing usable size and tracking the highest end address
- clear the bitmap to "all busy"
- paint all `BootMemoryType::Usable` pages free using a word-aligned span paint
- reserve the bitmap itself, everything below `kEarlyReservedPhysicalEnd` (the low-memory bootstrap scratch), and the kernel image range `[0x100000, 0x160000)`

`ReserveRange` is then used to pin any boot modules (initrd) before they are parsed.

### Virtual Memory

[`src/kernel/virtualmemory.h`](../src/kernel/virtualmemory.h) / [`src/kernel/virtualmemory.cpp`](../src/kernel/virtualmemory.cpp) manage 4-level paging. Current capabilities:

- lazy PML4 allocation on first use (`EnsureRoot`)
- `Allocate(start, num_pages, identity_map=bool)` for the old identity-map callers
- `MapPhysical(va, pa, count, flags)` for explicit physical mappings (e.g. MMIO for LAPIC/IOAPIC)
- `AllocateAndMap(va, count, flags)` for demand-allocating fresh physical pages with explicit flags
- `Protect(va, count, flags)` to change the flag bits on an existing mapping
- `Translate(va) -> (pa, flags)` for user-pointer validation during syscalls
- `CloneKernelPml4Entry(slot, source_root)` to share the kernel half of an address space
- `DestroyUserSlot(slot)` + `Free()` to tear down a per-process PML4 when a process exits
- `Activate()` to load the root into `CR3`

`PageFlags` is a typed enum with explicit `|` / `&` / `|=` overloads, and entries carry the NX bit in bit 63. The entry-address mask excludes bit 63 so masking the physical address never accidentally erases NX.

## CPU and SMP Architecture

### Per-CPU State

Per-CPU state is represented by `struct cpu` in [`src/kernel/cpu.h`](../src/kernel/cpu.h). Each `cpu` object occupies exactly one 4 KiB page and now contains:

- a self-pointer
- the current `Thread*` (replaces the old `Task*`)
- a per-CPU `TrapFrame` used when an interrupt arrives with no current thread
- a 7-entry GDT: null, kernel code/data, user code/data, and a 16-byte 64-bit TSS descriptor
- a per-CPU 64-bit TSS, whose `RSP0` is updated on every scheduling decision
- a linked-list pointer to the next CPU
- APIC ID, a `booted` flag used during AP bring-up, and a magic value for corruption checks
- the kernel stack in the remainder of the page

The layout is guarded by `CPU_STATIC_ASSERT` checks in the header so the NASM [`cpu.inc`](../src/kernel/cpu.inc) aliases (`CPU_CURRENT_THREAD`, `CPU_INTERRUPT_FRAME`, `CPU_TSS_RSP0`) match what the C++ struct produces.

`cpu_cur()` first reads `GS:0` (the self-pointer written by `cpu_init`); if GS has not been set yet it falls back to `RSP & ~0xFFF`, which works because each CPU's kernel stack and `cpu` object share the same page.

### CPU Initialization

[`src/kernel/cpu.cpp`](../src/kernel/cpu.cpp) is responsible for:

- populating the boot CPU from `cpu_boot_template` (which now includes user segments and a TSS slot)
- allocating additional CPU pages for APs through the page-frame allocator
- programming the TSS descriptor into the per-CPU GDT (`set_tss_descriptor`)
- loading each CPU's GDT, reloading segment registers, and loading the task register via `ltr CPU_GDT_TSS`
- setting MSR `0xC0000101` (GS base) to the current `cpu` structure

`cpu_set_kernel_stack(top)` writes the TSS `RSP0`, and the scheduler calls it on every context switch. That is what makes `int 0x80` from ring 3 land on the owning thread's kernel stack.

### Multiprocessor Discovery

[`src/kernel/mp.cpp`](../src/kernel/mp.cpp) still uses the legacy Intel MP specification (not ACPI). It scans the BIOS data area and `[0xF0000, 0xFFFFF)` for the `_MP_` floating pointer, validates the `PCMP` configuration table, records the LAPIC and IOAPIC addresses, and creates one `cpu` page per enabled processor entry.

Moving CPU/APIC topology onto ACPI `MADT` is a Milestone 4 deliverable, not something the current code supports. On a platform without MP tables the kernel treats itself as uniprocessor and boots the BSP only.

### AP Startup

Application processors are started through [`src/kernel/cpustart.asm`](../src/kernel/cpustart.asm). The boot CPU copies the trampoline to the fixed AP trampoline page (`kApTrampolineAddress`, currently `0x1000`) and fills the parameter block described in [`memory_layout.h`](../src/kernel/memory_layout.h):

| Address | Meaning |
| --- | --- |
| `kApStartupCpuPageAddress` (`0x20`) | pointer to the target CPU page |
| `kApStartupRipAddress` (`0x28`) | 64-bit function pointer to call after entering long mode |
| `kApStartupCr3Address` (`0x30`) | kernel `CR3` value to reuse |
| `kApStartupIdtAddress` (`0x38`) | 6-byte zero IDT pseudo-descriptor (so any NMI triple-faults loudly) |

Each AP:

- starts in 16-bit mode, masks PIC IRQs, loads the zero IDT
- enables PAE/PGE in CR4, then loads the kernel `CR3` directly (no separate AP page tables)
- enables both `LME` **and** `NXE` in `EFER` (matches the BSP boot path)
- turns on protected mode + paging in one `mov cr0`
- loads the shared GDT at physical `0`, jumps into 64-bit code
- points `RSP` at the top of its own CPU page and calls the supplied function (`init()` in `cpu.cpp`)

### AP Runtime State

Every AP runs `init()`, which calls `cpu_init()` (GDT/TSS/GS/MSRs), brings up its local APIC and IOAPIC bookkeeping, sets `booted = 1`, and then enters `cpu_idle_loop()` — an explicit `cli; hlt` loop rather than the previous ad-hoc `die()`. This is the Milestone 1 "controlled idle state" change.

APs are therefore **online but idle** today: they exist, they have a valid GDT/TSS/CR3 and an APIC ID, but the scheduler is still BSP-only. Joining APs to the scheduler is a deliberate follow-on milestone and will require finishing the IDT for APs and picking a locking story for the shared thread table.

## Interrupts, Exceptions, Syscalls

### Unified Trap Frame

[`src/kernel/trapframe.h`](../src/kernel/trapframe.h) defines the single `TrapFrame` layout used by IRQ, exception, syscall, and scheduler return paths. Its offsets are duplicated in [`trapframe.inc`](../src/kernel/trapframe.inc) and guarded by static asserts.

The same structure lives in two homes:

- embedded in `Thread` (`Thread::frame`) — used when a thread traps
- embedded in `cpu` (`cpu::interrupt_frame`) — used when an interrupt arrives with no current thread (e.g. before the scheduler is set up)

`trap_entry_common` in [`irqhandler.asm`](../src/kernel/irqhandler.asm) is the one assembly prologue for every kind of entry. It chooses the destination `TrapFrame` based on whether the current CPU has a current thread, writes all GPRs into it, preserves `vector`/`error_code`/`rip`/`cs`/`rflags` from the hardware-pushed frame, then branches on `CS.CPL` to decide whether `RSP`/`SS` were pushed by the CPU or need to be synthesized from the current kernel stack.

### IDT Construction

[`src/kernel/interrupt.cpp`](../src/kernel/interrupt.cpp) builds a 256-entry IDT:

- vectors `0x00-0x13` and `0x1D-0x1E` point to dedicated exception stubs from [`inthandler.asm`](../src/kernel/inthandler.asm)
- vectors `0x20-0x2F` point to IRQ stubs from [`irqhandler.asm`](../src/kernel/irqhandler.asm)
- vector `0x80` is wired to `int_80h` with type-attr `0xEE` — a ring-3 callable interrupt gate with DPL=3 — so user code can trap into the kernel via `int $0x80`
- The legacy PIC is remapped to `0x20-0x2F` even when the IOAPIC is also used

### Dispatch

All stubs jump into `trap_entry_common`, which calls `trap_dispatch(TrapFrame*)` in [`kernel.cpp`](../src/kernel/kernel.cpp#L736). `trap_dispatch` fans out:

- vectors `0x20` to `0x2F` go to `HandleIrq`
- vector `0x80` goes to `HandleSyscall`
- everything else goes to `HandleException`

`HandleIrq` runs a small registered-hook table for device IRQs (currently keyboard), acknowledges the LAPIC and both PICs, then returns either `current_thread` or the next thread to run (timer IRQ reschedules on every tick). `HandleException` distinguishes user-mode faults from kernel faults: user faults log, kill the offending thread, and call `ScheduleNext(false)` to continue; kernel faults dump the trap frame and halt the CPU.

### Return Path

`trap_dispatch` returns a `Thread*` that is the thread to resume. `trap_entry_common` inspects it:

- if the returned thread equals the caller's thread, the existing frame is resumed via `restore_frame_ptr`
- if it is a different thread, `restore_thread` is used: it writes `CPU::current_thread`, `TSS::RSP0` (so future kernel entries land on the right stack), and `CR3` (so the address space matches the thread), then `iretq`s into the new thread

`restore_frame_ptr` has two paths, chosen by the CS selector in the saved trap frame:

- user return (`CPL=3`): full `iretq` with CPU-provided `RSP`/`SS`
- kernel return (`CPL=0`): synthesises an `iretq` frame on the resumed stack using the saved `RIP` and `RFLAGS`, so even kernel threads resume through `iretq` semantics

### Syscall ABI

The syscall ABI deliberately matches the common Linux-style AMD64 layout so a future `SYSCALL`/`SYSRET` path can adopt it cheaply:

| Register | Role |
| --- | --- |
| `RAX` | syscall number on entry, return value on exit |
| `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` | arguments |

The numbering lives in [`src/kernel/syscall_abi.h`](../src/kernel/syscall_abi.h) and is mirrored in [`src/user/include/os1/syscall.h`](../src/user/include/os1/syscall.h):

| Number | Name | Kernel behavior |
| --- | --- | --- |
| 1 | `write(fd, buf, len)` | `fd` 1 and 2 write through both the serial debug channel and the active VGA terminal; user pointers are range-walked page-by-page through `Translate()` before any bytes are copied |
| 2 | `exit(status)` | marks the current thread Dying, schedules another thread |
| 3 | `yield()` | reschedules while keeping the current thread runnable |
| 4 | `getpid()` | returns the current process's PID |

Unknown syscalls return `-1`. No futex, signal, mmap, or fork exists at this milestone; that is intentional.

### APIC / PIC Roles

The kernel still uses all three interrupt controllers:

- [`pic.cpp`](../src/kernel/pic.cpp): remaps and mostly masks the legacy 8259 PIC
- [`ioapic.cpp`](../src/kernel/ioapic.cpp): routes external interrupts in MP mode
- [`lapic.cpp`](../src/kernel/lapic.cpp): enables the local APIC, sends EOIs, starts APs, and configures local vectors

When MP mode is active the kernel routes the PIT (input 2) and the keyboard (IRQ 1) through the IOAPIC. Everything else the legacy PIC used to handle is masked.

## Tasking and Scheduling

### Process and Thread Model

[`src/kernel/task.h`](../src/kernel/task.h) / [`src/kernel/task.cpp`](../src/kernel/task.cpp) now implement a real process/thread split instead of the old `Task`.

- `struct AddressSpace { uint64_t cr3; }` wraps per-process paging state.
- `struct Process` carries `pid`, lifecycle state (`Free`/`Ready`/`Running`/`Dying`), `AddressSpace`, exit status, and a short fixed-size name.
- `struct Thread` carries `tid`, a back-pointer to its `Process`, `ThreadState`, a `user_mode` flag, a cached `address_space_cr3`, `kernel_stack_base`/`kernel_stack_top`, an `exit_status`, and the embedded `TrapFrame`.

Both tables are capped statically (`kMaxProcesses = kMaxThreads = 32`) but live in page-frame-allocated memory, not at `0x408`. The `next` pointer inside `Thread` forms a runnable ring that is relinked on each scheduling decision. Thread-frame offsets are guarded by `THREAD_STATIC_ASSERT` checks in the header and mirrored in [`task.inc`](../src/kernel/task.inc) for the scheduler asm.

`initTasks(page_frames)`:

- allocates `kProcessTablePageCount` and `kThreadTablePageCount` pages
- zeroes them and marks every slot `Free`
- resets the per-CPU `current_thread`

### Creating a Kernel Thread

`createKernelThread(process, entry, frames)`:

- grabs a free slot
- allocates `kKernelThreadStackPages` (4) pages for the kernel stack
- reserves one dummy slot on top of the stack so the thread enters the C++ function with a SysV-correct alignment
- fills the embedded `TrapFrame` with kernel `CS`/`SS`, `RFLAGS = 0x202`, `RIP = entry`, and the shared kernel `CR3`
- links the thread into the runnable ring

The BSP idle thread is created this way and doubles as the scheduler's "there is nothing else to run" target.

### Creating a User Thread

`createUserProcess(name, cr3)` grabs a process slot and stamps the new PML4 root as its address space. `createUserThread(process, rip, rsp, frames)` then allocates a kernel stack for the thread (used on every syscall/interrupt entry), sets `user_mode = true`, and populates the thread's `TrapFrame` with user `CS`/`SS` (`kUserCodeSegment = 0x23`, `kUserDataSegment = 0x1B`), `RFLAGS = 0x202`, and the user `RIP`/`RSP` produced by the ELF loader.

### ELF Loader and initrd

`LoadUserProgram(path)` in [`kernel.cpp`](../src/kernel/kernel.cpp) walks the cpio newc archive described by `BootInfo.modules[0]`, finds the requested file, then calls `LoadUserElf()`:

- validates `ELF64` / `ET_EXEC` / `EM_x86_64`
- clones kernel PML4 slot 0 into the new user PML4 (shared kernel mappings)
- for each `PT_LOAD`:
  - rejects segments outside `[kUserSpaceBase, kUserStackTop - kUserStackPages*kPageSize)` or outside the expected user PML4 slot
  - allocates pages with flags derived from the program header (`Write` for writable segments, `NoExecute` when not executable, always `User`)
  - copies `filesz` bytes into the mapped range; remaining `memsz - filesz` bytes are left zero
- allocates the 16-page user stack (non-executable, writable, user) and leaves the page below as an unmapped guard
- translates `kUserStackTop - 8` to confirm the stack is actually reachable before returning
- sets `user_rsp = kUserStackTop - sizeof(uint64_t)` so the first user entry lines up with the SysV function-entry stack shape

The kernel currently loads three user programs and chains their threads onto the runnable ring:

- [`src/user/programs/init.c`](../src/user/programs/init.c) — prints a hello, yields three times, exits cleanly
- [`src/user/programs/yield.c`](../src/user/programs/yield.c) — prints three tick lines around explicit yields
- [`src/user/programs/fault.c`](../src/user/programs/fault.c) — deliberately dereferences `0x1234` so the smoke test can prove the kernel survives a user fault

The user-space runtime (`crt0` + syscall shim) is in [`src/user/lib/`](../src/user/lib/) and uses `int $0x80` to cross into the kernel. User ELFs are linked with `-mcmodel=large` at `0x0000008000400000` ([`src/user/linker.ld`](../src/user/linker.ld)); that combination keeps the ELFs relocatable to the high-canonical user slot without relying on small-model 32-bit absolute relocations.

The cpio archive is built at configure time from `build/user-root/` using the standard `cpio -o -H newc` format ([`cmake/scripts/create_initrd.cmake`](../cmake/scripts/create_initrd.cmake)) and laid into the disk image at `OS1_INITRD_IMAGE_START_LBA`.

### Scheduler Path

The scheduler is driven by the legacy PIT, programmed to 1 kHz in `SetTimer(1000)`. [`multitask.asm`](../src/kernel/multitask.asm) provides three entry points, all sharing the same `TrapFrame` shape:

- `startMultiTask(Thread*)` — called once at boot; jumps into `restore_thread`
- `restore_thread` — commits `current_thread`, `TSS::RSP0`, `CR3`, then resumes the thread's frame
- `restore_frame_ptr` — synthesizes either a user `iretq` or a kernel `iretq` frame on the outgoing stack, restores all GPRs, and returns into the thread

The rest of scheduling happens in C++ in [`kernel.cpp`](../src/kernel/kernel.cpp):

- `ScheduleNext(keep_current)` first reaps dying threads (`reapDeadThreads`), then optionally marks the current thread ready, then picks the next runnable thread via `nextRunnableThread()`; if everything is dying, it falls back to the idle thread
- `nextRunnableThread(after)` walks the static thread table in round-robin order, skipping `Free`/`Dying`/`Blocked` slots and deferring to the idle thread only if it is the sole runnable option
- `setCurrentThread(next)` writes the per-CPU slot, flips the thread state to `Running`, and updates the TSS `RSP0` so the next syscall lands on the right stack

On every PIT interrupt `HandleIrq` calls `ScheduleNext(true)`; on a syscall that returns to the same thread, `HandleSyscall` returns the same thread; on `exit`, it returns `ScheduleNext(false)` so the dying thread is not re-queued.

### Thread Teardown

`reapDeadThreads(page_frames)` frees dying threads one scheduling tick after they stop running. It:

- skips the currently executing thread (so the CPU never frees the stack it is still standing on)
- frees the kernel-stack pages
- clears the `Thread` slot
- if this was the last thread of a user process, tears down the process's user PML4 slot (`DestroyUserSlot(kUserPml4Index)`), frees the PML4 root page, and clears the `Process` slot
- relinks the runnable ring afterwards

This is what lets `/bin/fault` page-fault and `/bin/init` exit without leaking any of the page-frame, stack, or address-space resources.

## Console, Input, and Observability

### Serial Debug Output

[`src/kernel/debug.cpp`](../src/kernel/debug.cpp) is unchanged in purpose: polled 115200 baud COM1, usable very early, with the chained `debug("text")(value, base)()` API. The run target pipes serial output to `build/artifacts/os1.log`, and the smoke target captures it for marker matching.

### VGA Text Terminals

[`src/kernel/terminal.cpp`](../src/kernel/terminal.cpp) implements 12 simple 80x25 terminals. Each terminal backs a 4 KiB off-screen buffer allocated from the page-frame allocator; exactly one is "linked" to physical VGA memory at `0xB8000`. Terminal switching copies the target buffer into VGA memory. `F1`-`F10`, `F11`, `F12` scan codes switch the active terminal.

### Keyboard Path

[`src/kernel/keyboard.cpp`](../src/kernel/keyboard.cpp) handles PS/2 keyboard input through IRQ 1. The flow is the same as before:

1. IRQ1 fires.
2. The keyboard IRQ handler reads the scan code from port `0x60`.
3. Shift state is updated.
4. `KernelKeyboardHook()` in [`kernel.cpp`](../src/kernel/kernel.cpp) sees the raw scan code first and may switch the active terminal.
5. If the hook returns `true`, the keyboard layer maps the scan code to ASCII and delivers it to the active terminal.

`Terminal::ReadLn()` is a blocking helper used by some kernel-side demos; it is not exposed to userland.

## Testing and CI

### Smoke Target

[`CMakeLists.txt`](../CMakeLists.txt) registers a `ctest` test named `os1_smoke` that:

- runs QEMU headless with `-serial stdio -display none -no-reboot -no-shutdown`
- captures the full serial stream to `build/artifacts/smoke.log`
- searches the stream for a fixed list of markers:
  - `[kernel64] hello!`
  - `initializing page frame allocator`
  - `Interrupts initialization successful`
  - `initrd module discovered`
  - `starting first user process`
  - `[user/init] hello`
  - `[user/yield] tick 0`
  - `user page fault killed pid`
  - `idle thread online`
- fails if any marker is missing or QEMU times out

A convenience target `cmake --build build --target smoke` runs the test through `ctest --output-on-failure`.

### GitHub Actions

[`.github/workflows/ci.yml`](../.github/workflows/ci.yml) runs the same pipeline on Ubuntu 24.04:

- installs `nasm`, `qemu-system-x86`, `cmake`, `ninja` via APT
- installs `x86_64-elf-gcc` through Homebrew (Linuxbrew) on the runner
- configures with the `default` CMake preset
- builds the image
- runs the smoke test via `ctest`

The workflow also supports local runs through `act`, in which case it reuses the host toolchain instead of reinstalling it in a container.

## Current Architectural Boundaries

The system has clear limits today; these are not bugs, they are the deliberate scope of M1+M2:

- BIOS boot only; no UEFI path yet (Milestone 3).
- The BIOS loader is fixed-LBA and raw-sector based; it does not parse a partition table or filesystem.
- The ELF kernel loader is minimal (first `PT_LOAD`, no segment iteration, no relocations).
- Kernel virtual addresses are identity-mapped physical addresses; there is no higher half.
- CPU/APIC discovery depends on legacy MP tables; ACPI MADT is a Milestone 4 deliverable.
- APs come up, initialize locally, set `booted`, and enter `cpu_idle_loop()`; they do not yet run the scheduler.
- There is no block-device or filesystem driver post-boot; user programs come from the in-memory cpio archive.
- The syscall surface is `write` (fds 1 and 2), `exit`, `yield`, and `getpid`. No `mmap`, `fork`, `execve`, or file descriptors.
- The scheduler is a small BSP-only round-robin ring, not a general process subsystem.
- The console path is not SMP-safe (global `active_terminal`, global shift state); this is fine for BSP-only execution.
- `int 0x80` is used for syscall entry. `SYSCALL`/`SYSRET` is a later optimisation.

In other words: `os1` now has a coherent boot chain, a modern boot-handoff contract, and a small but real protected userland. The next structural steps are a modern default boot path (Limine + UEFI in Milestone 3) and modern platform discovery (ACPI, PCIe, virtio in Milestone 4).

## Related Files

If you want to follow the architecture in code, start here:

- Boot sector: [`src/boot/boot.asm`](../src/boot/boot.asm)
- 16-bit loader + 64-bit ELF expansion: [`src/boot/kernel16.asm`](../src/boot/kernel16.asm)
- EDD disk reads: [`src/boot/disk16_range.asm`](../src/boot/disk16_range.asm)
- E820 memory map: [`src/boot/biosmemory.asm`](../src/boot/biosmemory.asm)
- Long-mode switch: [`src/boot/long64.asm`](../src/boot/long64.asm)
- Boot handoff contract: [`src/kernel/bootinfo.h`](../src/kernel/bootinfo.h), [`src/kernel/bootinfo.cpp`](../src/kernel/bootinfo.cpp)
- Central memory layout: [`src/kernel/memory_layout.h`](../src/kernel/memory_layout.h), [`src/kernel/memory_layout.inc`](../src/kernel/memory_layout.inc)
- Kernel entry: [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp)
- CPU model: [`src/kernel/cpu.h`](../src/kernel/cpu.h), [`src/kernel/cpu.cpp`](../src/kernel/cpu.cpp)
- Physical memory: [`src/kernel/pageframe.cpp`](../src/kernel/pageframe.cpp)
- Virtual memory and flags: [`src/kernel/virtualmemory.h`](../src/kernel/virtualmemory.h), [`src/kernel/virtualmemory.cpp`](../src/kernel/virtualmemory.cpp)
- Interrupts/exceptions/syscall gate: [`src/kernel/interrupt.cpp`](../src/kernel/interrupt.cpp), [`src/kernel/irqhandler.asm`](../src/kernel/irqhandler.asm), [`src/kernel/inthandler.asm`](../src/kernel/inthandler.asm)
- Unified trap frame: [`src/kernel/trapframe.h`](../src/kernel/trapframe.h), [`src/kernel/trapframe.inc`](../src/kernel/trapframe.inc)
- MP/APIC code: [`src/kernel/mp.cpp`](../src/kernel/mp.cpp), [`src/kernel/lapic.cpp`](../src/kernel/lapic.cpp), [`src/kernel/ioapic.cpp`](../src/kernel/ioapic.cpp), [`src/kernel/cpustart.asm`](../src/kernel/cpustart.asm)
- Tasking and scheduler: [`src/kernel/task.h`](../src/kernel/task.h), [`src/kernel/task.cpp`](../src/kernel/task.cpp), [`src/kernel/multitask.asm`](../src/kernel/multitask.asm)
- Syscall ABI: [`src/kernel/syscall_abi.h`](../src/kernel/syscall_abi.h)
- User runtime: [`src/user/lib/crt0.c`](../src/user/lib/crt0.c), [`src/user/lib/syscall.c`](../src/user/lib/syscall.c), [`src/user/include/os1/syscall.h`](../src/user/include/os1/syscall.h)
- User programs: [`src/user/programs/init.c`](../src/user/programs/init.c), [`src/user/programs/yield.c`](../src/user/programs/yield.c), [`src/user/programs/fault.c`](../src/user/programs/fault.c)
- Console and input: [`src/kernel/terminal.cpp`](../src/kernel/terminal.cpp), [`src/kernel/keyboard.cpp`](../src/kernel/keyboard.cpp), [`src/kernel/debug.cpp`](../src/kernel/debug.cpp)
- Smoke test and CI: [`cmake/scripts/run_smoke_test.cmake`](../cmake/scripts/run_smoke_test.cmake), [`.github/workflows/ci.yml`](../.github/workflows/ci.yml)
