# os1 Architecture

> generated-by: Codex (GPT-5) · generated-at: 2026-04-21 · git-commit: `bcce101c653f4cc16b61ffa1ef3b6ebd35ffc06e`

This document describes the current architecture of `os1` as implemented in the repository today. It is meant to explain how the system boots, how control flows into the kernel, how memory and CPUs are organized, and how the main kernel subsystems fit together.

`os1` is a BIOS-booted, freestanding x86_64 hobby OS with:

- a 512-byte MBR boot sector
- a 16-bit second-stage loader
- a direct transition into 64-bit long mode
- a simple ELF64 kernel loader
- a freestanding C++ kernel with a few assembly components
- VGA text output, serial debug output, PS/2 keyboard input
- a bitmap physical page allocator
- an identity-mapped 4-level page-table manager
- legacy Intel MP-table based CPU discovery
- a simple round-robin kernel task scheduler

This is the current implementation architecture, not a target-state design.

## High-Level View

```text
BIOS
  -> boot sector (src/boot/boot.asm)
  -> 16-bit loader (src/boot/kernel16.asm)
  -> temporary long-mode transition (src/boot/long64.asm)
  -> 64-bit ELF relocation/copy step
  -> KernelMain(...) in src/kernel/kernel.cpp
       -> boot CPU setup
       -> physical page allocator
       -> MP discovery / AP startup
       -> kernel page tables
       -> PIC / IOAPIC / LAPIC setup
       -> terminals + keyboard
       -> IDT + exceptions + IRQ dispatch
       -> task creation + PIT-driven scheduler
```

## Repository Structure

| Path | Role |
| --- | --- |
| [`src/boot/`](../src/boot/) | BIOS boot sector, 16-bit loader, long-mode switch, early console and disk routines |
| [`src/kernel/`](../src/kernel/) | 64-bit kernel core, CPU/APIC code, interrupts, memory management, terminals, keyboard, tasks |
| [`src/libc/`](../src/libc/) | Small freestanding support routines used by the kernel |
| [`src/memory.txt`](../src/memory.txt) | Historical memory-layout notes; useful context but not a complete source of truth |
| [`CMakeLists.txt`](../CMakeLists.txt) | Build graph that produces the raw disk image and helper targets |

## Disk Image Layout

The raw image is assembled as a fixed-layout BIOS disk:

| LBA | Contents | Size | Produced from |
| --- | --- | --- | --- |
| 0 | MBR boot sector | 512 bytes | [`src/boot/boot.asm`](../src/boot/boot.asm) |
| 1-8 | 16-bit loader image | 8 sectors / 4 KiB | [`src/boot/kernel16.asm`](../src/boot/kernel16.asm) |
| 9-264 | kernel ELF payload slot | 256 sectors / 128 KiB reserved | kernel ELF build output |

The current loader assumes this layout directly. There is no partition table parsing, filesystem, or general-purpose stage loader.

## Boot Pipeline

### 1. BIOS and MBR Stage

[`src/boot/boot.asm`](../src/boot/boot.asm) is loaded by BIOS at `0x7C00`. Its responsibilities are intentionally small:

- set up real-mode segment registers and a temporary stack
- save the BIOS boot drive number from `DL`
- print a small status banner through the 16-bit console helper
- read 8 sectors starting at LBA 1 into physical address `0x1000`
- verify the loader signature word `0x7733`
- jump to the second-stage loader entry point

This stage is strictly an MBR loader. It knows nothing about ELF, paging, CPUs, or the kernel proper.

### 2. 16-bit Loader Stage

[`src/boot/kernel16.asm`](../src/boot/kernel16.asm) is loaded at `0x1000`. This stage performs the real-mode platform bring-up needed before entering 64-bit code.

Main responsibilities:

- enable and verify the A20 line
- verify long-mode support through CPUID
- read the kernel ELF image from disk into low memory at `kernel_image = 0x10000`
- capture the current VGA cursor position from BIOS interrupt `0x10`
- collect an E820 memory map via [`src/boot/biosmemory.asm`](../src/boot/biosmemory.asm)
- build a packed `SystemInformation` structure for the kernel
- allocate a temporary 16 KiB page-table area at `0xA000`
- jump into the long-mode transition routine in [`src/boot/long64.asm`](../src/boot/long64.asm)

The loader currently reads the kernel in four fixed 64-sector chunks from LBAs `9`, `73`, `137`, and `201`. This matches the fixed raw-image layout rather than a dynamic executable loader.

The system-information block passed to the kernel contains:

- saved VGA cursor X/Y
- the number of E820 memory blocks
- a pointer to the low-memory E820 block array

That C-facing definition lives in [`src/kernel/sysinfo.h`](../src/kernel/sysinfo.h).

### 3. Temporary Long-Mode Transition

[`src/boot/long64.asm`](../src/boot/long64.asm) performs the direct real-mode to long-mode transition.

Important details:

- it creates a temporary 4-level paging structure in the 16 KiB buffer pointed to by `ES:DI`
- only the first 2 MiB are identity-mapped at this stage
- interrupts are masked and a zero-length IDT is loaded during the transition
- it enables PAE/PGE in `CR4`
- it loads the temporary PML4 into `CR3`
- it sets the `LME` bit in `EFER`
- it enables protected mode + paging in `CR0`
- it copies a tiny GDT to physical address `0x0` and performs the far jump into 64-bit mode

This stage exists only to enter 64-bit code safely. It is replaced later by the kernel's own page tables.

### 4. 64-bit Loader / ELF Expansion

Once in long mode, control moves to `loader_main64` in [`src/boot/kernel16.asm`](../src/boot/kernel16.asm).

This code performs a minimal ELF load:

- it parses the ELF header fields it needs directly from the in-memory file image
- it reads the first program-header entry
- it clears the target memory at the segment virtual address
- it copies the segment bytes into place
- it zero-fills the remaining `memsz - filesz` tail, effectively creating `.bss`

This is a deliberately simple ELF loader:

- it assumes the kernel can be loaded from the first program header
- it does not implement a general-purpose multi-segment ELF loader
- it expects the kernel to be linked for physical/virtual address `0x100000`

The link address comes from [`src/kernel/linker2.ld`](../src/kernel/linker2.ld), which sets:

- entry point: `KernelMain(SystemInformation*, cpu*)`
- `.text` base: `0x100000`

Before transferring control, the loader also derives the initial boot CPU page and stack:

- it rounds the loaded kernel end up to the next 4 KiB boundary
- it uses that aligned page as the first `cpu` structure
- it sets `RSP` to the top of that page
- it calls the ELF entry point with:
  - `RDI = system_info`
  - `RSI = cpu_boot`

## Kernel Entry and Runtime Flow

The kernel entrypoint is [`KernelMain`](../src/kernel/kernel.cpp) in [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp).

Its current initialization sequence is:

1. Deep-copy the `SystemInformation` block and E820 array from low memory into kernel-owned stack data.
2. Initialize the boot CPU page from `cpu_boot_template`.
3. Initialize the physical page-frame allocator from the E820 map.
4. Discover processors and APIC information through legacy Intel MP tables.
5. Build kernel page tables and identity-map the kernel's working physical address ranges.
6. Activate the new page tables by loading the kernel PML4 into `CR3`.
7. Initialize the PIC, I/O APIC, and local APIC.
8. Boot the application processors (APs).
9. Allocate terminal backing pages and attach terminal 0 to VGA memory.
10. Build and load the IDT, then register exception handlers.
11. Register the PS/2 keyboard handler.
12. Create three demo kernel tasks and enter the multitasking loop.

The boot CPU continues through all of this. The APs are brought up, but today they stop after their local CPU/APIC setup instead of joining the scheduler.

## Memory Architecture

### Memory Model Overview

The kernel uses a mixture of:

- fixed low physical addresses for bootstrap data and early runtime structures
- dynamic 4 KiB pages allocated from the page-frame allocator
- identity mapping for kernel virtual memory

There is no higher-half kernel mapping. In the current design, kernel virtual addresses are the same as physical addresses for the mapped ranges.

### Important Physical Addresses

These addresses are hard-coded into the current implementation:

| Address / Range | Current role |
| --- | --- |
| `0x0000` | Temporary GDT placement during long-mode entry and AP startup |
| `0x0020-0x003F` | AP startup parameter block consumed by [`cpustart.asm`](../src/kernel/cpustart.asm) |
| `0x0400` | BIOS data area is read during MP discovery; task storage also begins at `0x408` later |
| `0x1000` | 16-bit loader location during boot; later reused as the AP trampoline location |
| `0xA000-0xDFFF` | Temporary real-mode page tables for entering long mode |
| `0x10000` | Low-memory kernel ELF load buffer during stage-2 boot |
| `0x20000-0x5FFFF` | Reserved for the page-frame bitmap in current kernel runtime layout |
| `0x60000` | First free page after the bitmap; currently becomes the first kernel PML4 root |
| `0x100000-0x15FFFF` | Kernel code/data reservation marked busy by the page allocator |

Two details are especially important:

- `taskList` is hard-coded at physical address `0x408` in [`src/kernel/task.cpp`](../src/kernel/task.cpp).
- the AP trampoline parameters are written directly to low memory before each AP startup in [`src/kernel/cpu.cpp`](../src/kernel/cpu.cpp).

The system still relies on low memory as a shared bootstrap scratchpad.

### Physical Page Allocation

[`src/kernel/pageframe.cpp`](../src/kernel/pageframe.cpp) implements the physical page allocator.

Key properties:

- page size is fixed at 4 KiB
- the allocator uses one bitmap bit per physical page
- a bit value of `1` means free/usable
- a bit value of `0` means reserved or already allocated
- the allocator seeds the bitmap from the E820 map

Initialization flow:

- sum all E820 type-1 regions to compute usable memory
- find the highest end address to compute the page count
- clear the bitmap to "all busy"
- mark all E820 usable pages as free
- then mark internal kernel-reserved ranges busy again:
  - the bitmap storage itself
  - low kernel/bootstrap pages below `0x20000`
  - kernel image pages from `0x100000` to `0x160000`

The bitmap is placed at `0x20000`, and the kernel currently reserves `0x40000` bytes of low memory for it, covering up to 64 GiB worth of 4 KiB pages.

### Virtual Memory

[`src/kernel/virtualmemory.cpp`](../src/kernel/virtualmemory.cpp) manages page tables.

Current design:

- 4-level x86_64 paging
- 4 KiB pages only
- no huge pages
- no user/kernel split
- no higher-half mapping
- kernel boot path uses identity mapping almost exclusively

`VirtualMemory::Allocate(start, num_pages, identity_map)`:

- lazily allocates a root PML4 on first use
- allocates intermediate page-table levels as needed
- when `identity_map` is `true`, maps each virtual page to the same physical address
- otherwise, allocates backing physical pages from the page-frame allocator

During boot, `KernelMain` creates a kernel address space that identity-maps:

- the first 1 MiB
- every E820 usable region above 1 MiB
- the LAPIC MMIO page
- the IOAPIC MMIO page

Once populated, the kernel activates this address space with `mov %root, %cr3`.

## CPU and SMP Architecture

### Per-CPU State

Per-CPU state is represented by `struct cpu` in [`src/kernel/cpu.h`](../src/kernel/cpu.h).

Each `cpu` object occupies exactly one 4 KiB page and contains:

- a self-pointer
- the current scheduled `Task*`
- a saved `Registers` block used by interrupt/exception code
- a 3-entry GDT (null, kernel code, kernel data)
- a linked-list pointer to the next CPU
- the APIC ID
- a `booted` flag for AP startup synchronization
- a magic value used for basic corruption checks
- the CPU's kernel stack in the remainder of the page

The current CPU is found by:

- reading `GS:0` after `cpu_init()` has written the GS base MSR, or
- falling back to `RSP & ~0xFFF`, because each CPU stack lives in the same page as the `cpu` structure

### CPU Initialization

[`src/kernel/cpu.cpp`](../src/kernel/cpu.cpp) is responsible for:

- populating the boot CPU from `cpu_boot_template`
- allocating additional CPU pages for APs through the page-frame allocator
- loading each CPU's GDT
- reloading segment registers
- setting MSR `0xC0000101` (GS base) to the current `cpu` structure

### Multiprocessor Discovery

[`src/kernel/mp.cpp`](../src/kernel/mp.cpp) uses the legacy Intel MP specification, not ACPI.

The discovery path:

- scans the BIOS data area and BIOS ROM for the `_MP_` floating pointer structure
- validates the `PCMP` configuration table
- records the LAPIC base address from the MP table
- creates one `cpu` structure per enabled processor entry
- records the IOAPIC ID and address from the MP table

This means the current architecture depends on legacy MP tables being present.

### AP Startup

Application processors are started through [`src/kernel/cpustart.asm`](../src/kernel/cpustart.asm).

The boot CPU copies the trampoline to `0x1000` and fills a shared parameter block:

| Address | Meaning |
| --- | --- |
| `0x20` | pointer to the target CPU page |
| `0x28` | 64-bit function pointer to call after entering long mode |
| `0x30` | kernel `CR3` value to reuse |
| `0x38` | temporary IDT pseudo-descriptor |

The AP trampoline:

- starts in 16-bit mode
- disables interrupts
- reuses the already-built kernel page tables by loading the supplied `CR3`
- enters long mode
- sets up segment registers
- points `RSP` to the top of the supplied CPU page
- calls the supplied function pointer (`init()` in `cpu.cpp`)

Current AP behavior is intentionally limited:

- APs run `cpu_init()`
- initialize IOAPIC/LAPIC state locally
- set their `booted` flag
- then halt inside `die()`

So the system is multiprocessor-aware, but not yet a fully SMP-running kernel.

## Interrupt and Exception Architecture

### IDT Construction

[`src/kernel/interrupt.cpp`](../src/kernel/interrupt.cpp) builds a 256-entry IDT and loads it with `lidt`.

Current vector layout:

- `0x20-0x2F` -> remapped hardware IRQs
- `0x20` specifically points to the task-switch interrupt path
- selected CPU exception vectors point to dedicated assembly stubs

The kernel currently remaps the legacy PIC to `32-47` even when the IOAPIC is also used.

### IRQ Dispatch

Hardware IRQ flow is split between:

- assembly stubs in [`src/kernel/irqhandler.asm`](../src/kernel/irqhandler.asm)
- the C dispatch table in [`src/kernel/ihandler.c`](../src/kernel/ihandler.c)

The assembly layer:

- disables interrupts
- saves general-purpose register state via `saveregs`
- pushes the IRQ number
- calls `irq_handler(number)`
- acknowledges the LAPIC
- acknowledges the PIC(s)
- restores registers
- returns with `iretq`

The C layer stores 16 function pointers and opaque data pointers, so device drivers can register per-IRQ callbacks.

### Exception Dispatch

CPU exception handling is similarly split:

- assembly entry stubs in [`src/kernel/inthandler.asm`](../src/kernel/inthandler.asm)
- a C exception-handler table in [`src/kernel/ihandler.c`](../src/kernel/ihandler.c)
- high-level handlers in [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp)

Important detail: `saveregs` and `restoreregs` store the current register image in the current CPU's `interrupt_regs` field via `GS`, so exception state is per-CPU.

The top-level exception handlers in `kernel.cpp` currently:

- print a human-readable exception name
- dump RIP/RSP/error-code/CR2/CR3
- print the saved register block
- halt the CPU

### APIC and PIC Roles

The project currently uses all three interrupt controllers in different roles:

- [`src/kernel/pic.cpp`](../src/kernel/pic.cpp): remaps and mostly masks the legacy 8259 PIC
- [`src/kernel/ioapic.cpp`](../src/kernel/ioapic.cpp): routes external interrupts in MP mode
- [`src/kernel/lapic.cpp`](../src/kernel/lapic.cpp): enables the local APIC, sends EOIs, starts APs, and configures local vectors

When MP mode is detected, the kernel enables:

- timer routing through IOAPIC input 2
- keyboard routing through the IOAPIC keyboard line

## Console, Input, and Observability

### Serial Debug Output

[`src/kernel/debug.cpp`](../src/kernel/debug.cpp) provides the kernel's serial debug channel on COM1.

Properties:

- 115200 baud
- polled output
- used very early and throughout bring-up
- supports a chained `debug("text")(value, base)()` style API

This is the main observability channel captured in `build/artifacts/os1.log`.

### VGA Text Terminals

[`src/kernel/terminal.cpp`](../src/kernel/terminal.cpp) implements simple 80x25 text terminals.

Current design:

- the kernel creates 12 terminal buffers
- each terminal gets a dedicated 4 KiB page from the page-frame allocator
- only one terminal is "linked" to physical VGA text memory at `0xB8000`
- inactive terminals keep off-screen backing buffers
- switching terminals copies the target buffer into VGA memory

Each `Terminal` tracks:

- its backing buffer
- whether it is currently attached to the real screen
- cursor row/column
- a single-character input slot used by `ReadLn`

### Keyboard Path

[`src/kernel/keyboard.cpp`](../src/kernel/keyboard.cpp) handles PS/2 keyboard input through IRQ1.

Flow:

1. IRQ1 fires.
2. The keyboard IRQ handler reads the scan code from port `0x60`.
3. Shift state is updated.
4. `KernelKeyboardHook()` in [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp) sees the raw scan code first.
5. Function keys `F1-F12` switch the active terminal.
6. If the key is still accepted, the keyboard layer translates the scan code to ASCII.
7. The translated character is delivered to the active terminal through `Terminal::KeyPress`.

`Terminal::ReadLn()` is a blocking kernel-side line input helper built on top of that single-character slot.

## Tasking and Scheduling

### Task Model

The scheduler is a simple kernel-thread model defined by [`src/kernel/task.h`](../src/kernel/task.h) and [`src/kernel/task.cpp`](../src/kernel/task.cpp).

Each `Task` contains:

- a `nexttask` link for the round-robin ring
- a PID
- a timer field
- a saved `Registers` structure

Tasks are allocated from a fixed array:

- `taskList` lives at physical address `0x408`
- capacity is 32 tasks
- there is no dynamic task allocation structure beyond that static array

### Execution Model

Tasks are currently:

- ring-0 only
- kernel functions, not user processes
- all sharing the same kernel address space
- bootstrapped by constructing an initial `iretq` frame on a private stack page

`newTask()`:

- allocates a free `Task` slot
- fills its saved register block
- points its `RSP` at the synthetic interrupt frame
- records the task pointer in `R15`
- hardcodes `CR3 = 0x60000`, matching the current kernel page-table root layout
- links the task into a circular singly-linked list

### Scheduler Path

Scheduling is driven by the legacy PIT, not by a user-mode process abstraction.

[`src/kernel/multitask.asm`](../src/kernel/multitask.asm) provides two key entry points:

- `startMultiTask(Task*)`
- `task_switch_irq`

`startMultiTask()`:

- disables interrupts
- stores the current task in the per-CPU `current_task`
- programs the PIT divisor to `1193` ticks, roughly 1 kHz
- restores the saved task register image
- enters the task with `iretq`

`task_switch_irq`:

- runs on vector `0x20`
- saves the current task's register state
- advances to `current_task->nexttask`
- acknowledges the interrupt controllers
- restores the next task's register state
- returns into the next task with `iretq`

The demo workload in [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp) creates three kernel tasks:

- `process1()` writes `1`
- `process2()` writes `2`
- `process3()` interacts with terminal 3 and can block on `ReadLn`

## Current Architectural Boundaries

The current system has a clear shape, but also some strong constraints:

- BIOS boot only; there is no UEFI path.
- The boot loader is fixed-layout and raw-sector based.
- The ELF loader is minimal and assumes the current kernel image structure.
- The runtime address space is identity-mapped; there is no higher-half kernel.
- The system is kernel-only: no ring-3, syscalls, or process isolation.
- AP startup works, but APs do not yet join normal kernel execution after initialization.
- CPU discovery depends on legacy Intel MP tables rather than ACPI.
- There is no filesystem or block-device driver after boot.
- The scheduler is a small in-kernel round-robin mechanism, not a general process subsystem.

In other words: `os1` already has a coherent boot chain and kernel core, but it is still an early, direct, hardware-near architecture built around fixed physical addresses and synchronous bring-up.

## Related Files

If you want to follow the architecture in code, these are the best starting points:

- Boot sector: [`src/boot/boot.asm`](../src/boot/boot.asm)
- 16-bit loader: [`src/boot/kernel16.asm`](../src/boot/kernel16.asm)
- Long-mode switch: [`src/boot/long64.asm`](../src/boot/long64.asm)
- Kernel entry: [`src/kernel/kernel.cpp`](../src/kernel/kernel.cpp)
- CPU model: [`src/kernel/cpu.h`](../src/kernel/cpu.h), [`src/kernel/cpu.cpp`](../src/kernel/cpu.cpp)
- Physical memory: [`src/kernel/pageframe.cpp`](../src/kernel/pageframe.cpp)
- Virtual memory: [`src/kernel/virtualmemory.cpp`](../src/kernel/virtualmemory.cpp)
- Interrupts: [`src/kernel/interrupt.cpp`](../src/kernel/interrupt.cpp), [`src/kernel/irqhandler.asm`](../src/kernel/irqhandler.asm), [`src/kernel/inthandler.asm`](../src/kernel/inthandler.asm)
- MP/APIC code: [`src/kernel/mp.cpp`](../src/kernel/mp.cpp), [`src/kernel/lapic.cpp`](../src/kernel/lapic.cpp), [`src/kernel/ioapic.cpp`](../src/kernel/ioapic.cpp), [`src/kernel/cpustart.asm`](../src/kernel/cpustart.asm)
- Console and input: [`src/kernel/terminal.cpp`](../src/kernel/terminal.cpp), [`src/kernel/keyboard.cpp`](../src/kernel/keyboard.cpp), [`src/kernel/debug.cpp`](../src/kernel/debug.cpp)
- Tasking: [`src/kernel/task.cpp`](../src/kernel/task.cpp), [`src/kernel/multitask.asm`](../src/kernel/multitask.asm)
