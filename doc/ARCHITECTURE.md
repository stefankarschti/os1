# os1 Architecture

> generated-by: Codex (GPT-5) · generated-at: 2026-04-22 · git-commit: `ccf881c8170e9645d51d6d734431bed836935c6f`

This document is the current-state source of truth for `os1`. It describes what is implemented in the repository today. The review documents under `doc/` are historical context, not the live system contract.

`os1` currently has:

- a shared-kernel, dual-entry boot architecture
- a default modern UEFI boot path based on Limine
- an explicit legacy BIOS compatibility path
- one kernel-facing boot contract: `BootInfo`
- a freestanding `C++20` kernel core with narrow assembly boundaries
- protected ring-3 user programs loaded from an initrd
- a terminal model that can render either through VGA text mode or a framebuffer text backend
- automated smoke coverage for both the UEFI and BIOS paths in CI

Milestone status:

- Milestone 1: implemented
- Milestone 2: implemented
- Milestone 3: implemented
- Milestone 4: planned

## Glossary

| Term | Meaning in `os1` |
| --- | --- |
| BIOS | Legacy PC firmware interface used by the compatibility boot path. |
| UEFI | Modern firmware interface used by the default boot path. |
| Limine | The bootloader / protocol used for the modern default path. |
| OVMF | The UEFI firmware image used by QEMU for the default `run` and `smoke` targets. |
| HHDM | Higher-Half Direct Map. Limine provides a virtual mapping of physical memory so the shim can access physical ranges before the kernel installs its own page tables. |
| `BootInfo` | The normalized bootloader-to-kernel contract. Every boot frontend must convert its native state into this structure before entering `KernelMain`. |
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

- `kernel16.bin` plus `boot.bin` for the legacy BIOS raw-image path
- `kernel_limine.elf` for the default Limine/UEFI path

The shared kernel core is:

- `kernel_bios.elf`

That naming reflects implementation history rather than long-term intent. `kernel_bios.elf` is not BIOS-only logic anymore. It is the low-half kernel core used by both paths. The Limine path loads it as a module and then transfers control into the same `KernelMain(BootInfo*, cpu*)` entry that the BIOS loader uses.

This split exists for a pragmatic reason: the kernel core is still linked at low identity-mapped addresses around `0x00100000`, while the Limine executable itself must be presented in a form the modern bootloader accepts. The higher-half Limine frontend exists to bridge that difference without teaching the kernel multiple boot ABIs.

## Build Outputs And Boot Artifacts

The CMake build now produces two image families:

- `build/artifacts/os1.iso`
  This is the default artifact. It is a UEFI-only Limine ISO.
- `build/artifacts/os1.raw`
  This is the explicit legacy BIOS compatibility image.

It also produces the boot payloads that feed those images:

- `boot.bin`
- `kernel16.bin`
- `kernel_bios.elf`
- `kernel_limine.elf`
- `initrd.cpio`
- `user/init.elf`, `user/yield.elf`, `user/fault.elf`

The ISO stages these files:

- `EFI/BOOT/BOOTX64.EFI`
- `limine.conf`
- `boot/limine/limine-uefi-cd.bin`
- `kernel_limine.elf`
- `kernel_bios.elf`
- `initrd.cpio`

The BIOS raw image keeps a fixed LBA layout generated at configure time and emitted into NASM through `cmake/templates/image_layout.inc.in`:

- LBA `0`: MBR boot sector
- LBA `1-64`: `kernel16.bin`
- LBA `65-320`: `kernel_bios.elf`
- LBA `321-448`: `initrd.cpio`

## The Shared Kernel Contract: `BootInfo`

`src/kernel/bootinfo.h` defines the only boot contract the kernel consumes.

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

The kernel immediately calls `OwnBootInfo()` on entry. That function deep-copies:

- the header
- the memory map
- the module list
- bootloader name
- command line
- per-module names

into kernel-owned BSS storage. That matters because bootloader staging memory is not a stable ownership boundary. Once `OwnBootInfo()` returns, the rest of the kernel no longer depends on bootloader-owned metadata buffers.

## Early Physical Layout Shared By Boot Paths

`src/kernel/memory_layout.h` and `src/kernel/memory_layout.inc` hold the fixed early-boot addresses used by the BIOS loader and mirrored by the Limine shim.

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

The modern path deliberately mirrors this layout before entering `KernelMain`. That is not nostalgia. It is a compatibility technique that keeps the shared kernel core bootloader-agnostic while the project still uses a low identity-linked kernel.

## Modern Default Boot Path: Limine + UEFI

### Firmware And Bootloader Flow

The default run path is:

1. QEMU starts `q35` with OVMF.
2. OVMF loads Limine from `EFI/BOOT/BOOTX64.EFI`.
3. Limine reads `limine.conf`.
4. Limine loads `kernel_limine.elf` as the executable and publishes `kernel_bios.elf` plus `initrd.cpio` as modules.
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

The shared kernel image is `kernel_bios.elf`, exposed by Limine as a module. The shim parses its ELF64 program headers and handles `PT_LOAD` segments only.

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

The goal is not to keep Limine's page tables forever. The goal is to make the final jump into `KernelMain` valid while preserving the rest of Limine's higher-half environment long enough for the kernel to copy `BootInfo` and build its own page tables.

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
- calls the shared low-half `KernelMain`

## Legacy BIOS Compatibility Path

The BIOS path remains in tree and is continuously tested.

Flow:

1. BIOS loads `boot.bin` at `0x7C00`.
2. The MBR stage loads the first sector of `kernel16.bin`.
3. `kernel16.bin` finishes loading itself through CHS reads.
4. The loader enables A20, checks long-mode support, reads the kernel and initrd through EDD packet reads, captures the BIOS cursor, and collects the E820 memory map.
5. `src/boot/long64.asm` builds temporary page tables, enables `LME` and `NXE`, and jumps into long mode.
6. The 64-bit loader expands `kernel_bios.elf` at `0x00100000`, builds `BootInfo`, allocates the boot CPU page, and calls `KernelMain`.

The BIOS path still exists for three reasons:

- it provides a low-complexity fallback path
- it preserves bring-up knowledge
- it is now cheap to keep because the kernel contract is shared

It is no longer the default workflow.

## Kernel Initialization After Handoff

`KernelMain()` in `src/kernel/kernel.cpp` is the shared kernel entry for both paths.

The high-level sequence is:

1. serial hello
2. `OwnBootInfo()`
3. boot CPU initialization
4. physical page-frame allocator initialization
5. reserve boot modules and framebuffer ranges
6. CPU discovery
7. build kernel-owned identity-mapped page tables
8. map boot-critical non-usable ranges such as initrd and framebuffer
9. switch to the kernel's `CR3`
10. initialize PIC, IOAPIC, LAPIC
11. start APs when available
12. allocate terminals
13. select display backend
14. initialize interrupts and keyboard
15. initialize scheduler tables
16. create kernel idle thread
17. load user programs from initrd
18. start multitasking

The important architectural point is that the boot frontends stop mattering almost immediately after `OwnBootInfo()`. After that point, the system is running on kernel-owned page tables and kernel-owned copies of boot metadata.

## Console And Display Architecture

Milestone 3 introduced a real split between the logical terminal model and the physical presentation backend.

### Terminal Model

`src/kernel/terminal.cpp` still owns the text-cell model:

- fixed 80x25 grid
- cursor tracking
- multiple terminal buffers
- write/scroll/clear behavior

The terminal remains fixed-size on both boot paths. That is intentional. It keeps the user-visible shell model stable while the display layer changes under it.

### Display Backends

`src/kernel/display.cpp` adds two presentation backends:

- `VgaTextDisplay`
- `FramebufferTextDisplay`

The active backend is chosen from `BootInfo`:

- BIOS defaults to VGA text mode
- Limine/UEFI uses the framebuffer backend when the format is supported
- otherwise the kernel continues with serial-only diagnostics

### Framebuffer Text Rendering

The framebuffer backend is intentionally minimal:

- fixed 80x25 text grid
- public-domain `font8x8` bitmap glyphs, doubled vertically to 8x16
- white text on black background
- software cursor by inverse cell rendering
- centered text region with padding on larger framebuffers
- full redraw on every present

This is not a full graphics subsystem. It is a presentation adapter for the existing terminal model.

The full-redraw policy is deliberate. It keeps the first framebuffer path easy to reason about and debug. Serial remains the authoritative debug channel.

## Memory Architecture

### Physical Page Allocation

`PageFrameContainer` owns the physical page allocator. It is bitmap-based and now seeds itself directly from `std::span<const BootMemoryRegion>` rather than an older BIOS-specific structure.

The allocator:

- marks all pages busy by default
- frees only `BootMemoryType::Usable` ranges
- reserves the bitmap itself
- reserves low bootstrap ranges
- reserves the kernel image reservation
- can reserve arbitrary physical ranges through `ReserveRange()`

That last capability matters on the modern path because initrd modules and framebuffers may live in memory-map regions that are not simple "usable RAM" entries.

### Virtual Memory

`VirtualMemory` manages kernel and user mappings. The kernel still uses low identity mappings, but user mappings now live in a dedicated PML4 slot:

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

The first syscall ABI uses `int 0x80` on vector `48`, configured as a user-callable interrupt gate.

Current syscalls:

- `write`
- `exit`
- `yield`
- `getpid`

This is intentionally small. The milestone goal was to establish protection boundaries and process lifecycle, not a large syscall surface.

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

The kernel validates user pointers through page-table translation. It does not rely on deliberate kernel faults as a normal syscall-copy mechanism.

### Initrd And ELF Loader

The initrd is a `cpio newc` archive built from `src/user`.

Current user programs:

- `/bin/init`
- `/bin/yield`
- `/bin/fault`

The kernel parses the initrd, finds those paths, and loads ELF64 `ET_EXEC` images with `PT_LOAD` segments only. It maps segment permissions from ELF flags and zero-fills `memsz - filesz` for `.bss`.

This first userland is intentionally a vertical slice, not a general process-launch environment. There is no filesystem-backed `exec`, no `fork`, no arguments/environment, and no shell yet.

### Process Fault Handling And Teardown

If a user process faults:

- the kernel detects ring 3 from the saved code segment
- logs the fault
- marks the thread and process dying
- schedules away
- reaps the dead process later from another stack

This deferred teardown is important. The kernel does not free the current thread's stack or address space while it is still executing on it.

## SMP And Current Limits

CPU discovery and AP bring-up are still based on legacy Intel MP tables, not ACPI MADT.

That has a visible consequence:

- the modern UEFI `q35` path boots successfully and reaches userland
- but current automated UEFI runs often remain effectively BSP-only because modern ACPI-based topology discovery is not implemented yet

That is not a Milestone 3 regression. It is the expected boundary between Milestone 3 and Milestone 4. The boot path is modernized; platform discovery is not yet.

APs that do start enter `cpu_idle_loop()`, an explicit interrupt-disabled `cli; hlt` loop. They are not yet part of full multi-CPU user scheduling.

## Test And CI Architecture

### Local Targets

The main CMake targets are:

- `os1_image`
- `os1_bios_image`
- `run`
- `run_bios`
- `smoke`
- `smoke_bios`
- `smoke_all`

`run` boots the default UEFI ISO under OVMF on `q35`. `run_bios` boots the raw image directly.

### Smoke Tests

`CTest` registers two boot-path tests:

- `os1_smoke`
- `os1_smoke_bios`

The modern smoke test verifies markers including:

- `boot source: limine`
- `framebuffer console active`
- initrd discovery
- user-process startup
- clean user fault handling
- idle-thread fallback

The BIOS smoke test verifies the equivalent legacy path markers.

### CI

GitHub Actions runs on `ubuntu-24.04` and does all of the following on every push and pull request:

- install host tools including `cpio`, `xorriso`, `ovmf`, `qemu-system-x86`
- install the `x86_64-elf` cross toolchain through Homebrew
- configure the project
- build the default modern artifact
- explicitly build the BIOS compatibility artifact
- run both smoke tests through `ctest`

The same single CI job name is kept for local `act` compatibility.

## Current Constraints And Next Step

The current architecture is coherent, but intentionally incomplete.

Major constraints that remain:

- the shared kernel core is still low identity-linked rather than higher-half
- modern CPU discovery still depends on Milestone 4 ACPI work
- the framebuffer path is a text presenter, not a graphics stack
- userland is still initrd-demo oriented rather than shell/filesystem oriented
- syscalls still use `int 0x80`, not `SYSCALL`/`SYSRET`

The next milestone is therefore not another boot refactor. It is modern platform support:

- ACPI MADT parsing
- modern timer/controller discovery
- PCI / PCIe discovery
- virtio-first device bring-up

At this point the architecture is intentionally in a good place for that work: the boot path is modernized, the kernel entry contract is shared, and both modern and legacy boot paths remain continuously testable.
