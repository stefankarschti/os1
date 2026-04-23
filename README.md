# os1

`os1` is a self-documented teaching and engineering operating system project: a small, technically serious `x86_64` OS built for clarity, runnable vertical slices, and modern OS concepts rather than feature sprawl. The project stays terminal-first, QEMU-first, and documentation-heavy on purpose, with the current implementation status described in [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) and the longer-term direction in [GOALS.md](GOALS.md).

Today `os1` uses a shared-kernel, dual-entry boot architecture. The default path is a Limine-based UEFI ISO that enters a thin higher-half shim, normalizes bootloader state into `BootInfo`, then transfers control to the shared low-half kernel core. The legacy BIOS raw image is still built and tested as a compatibility path. The kernel itself is freestanding `C++20`, runs protected user programs loaded from an initrd `cpio` archive, exposes `write`/`exit`/`yield`/`getpid` through `int $0x80`, and can present the terminal either through VGA text mode or a minimal framebuffer text renderer. Milestone 4 is now implemented: both boot paths populate `BootInfo.rsdp_physical`, the kernel discovers topology through ACPI `MADT`, enumerates PCIe through `MCFG`, probes a modern `virtio-blk` device, validates raw-sector reads from a generated test disk, and boots the default `q35` targets with ACPI-discovered AP startup.

## Prerequisites

Required build tools:

- `x86_64-elf-gcc`
- `x86_64-elf-g++`
- `x86_64-elf-ld`
- `nasm`
- `cpio`
- `xorriso`

Optional helper tools:

- `qemu-system-x86_64` for `run`, `run_bios`, and smoke tests
- OVMF / edk2 firmware images for the default UEFI path
- `x86_64-elf-objdump` or `objdump`

On macOS with Homebrew:

```sh
brew install x86_64-elf-gcc nasm cpio xorriso qemu
```

Homebrew `qemu` ships usable OVMF firmware files and the CMake build will auto-detect them. On Linux, install your distro's `ovmf` or `edk2-ovmf` package.

## Build

If you have an old Make-based or pre-CMake-M3 `build/` directory, remove it once before switching:

```sh
rm -rf build
```

Configure and build:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-elf.cmake
cmake --build build
```

The default build now produces the modern UEFI ISO:

- `build/artifacts/os1.iso`

Build the explicit legacy BIOS image when needed:

```sh
cmake --build build --target os1_bios_image
```

That produces:

- `build/artifacts/os1.raw`

## VS Code CMake Tools

This repo ships CMake presets for the supported workflows. In VS Code with the CMake Tools extension:

- configure with the `default` preset
- build with the `default` preset
- use the `run` preset or target for the default UEFI path
- use the `run_bios` preset or target for the legacy BIOS path
- use the `smoke`, `smoke_bios`, or `smoke_all` presets for tests

If the extension still has stale cache or generator state from an older setup, run `CMake: Delete Cache and Reconfigure` once.

## Run

Default modern UEFI path:

```sh
cmake --build build --target run
```

This boots OVMF on `q35` and attaches the generated `virtio-blk` test disk used by the modern-platform smoke.

Legacy BIOS compatibility path:

```sh
cmake --build build --target run_bios
```

This boots the raw image on BIOS under `q35` with the same secondary `virtio-blk` test disk attached.

## Smoke Tests

Modern UEFI smoke test:

```sh
cmake --build build --target smoke
```

Legacy BIOS smoke test:

```sh
cmake --build build --target smoke_bios
```

Run both:

```sh
cmake --build build --target smoke_all
```

Or run the registered CTest suite directly:

```sh
ctest --test-dir build --output-on-failure
```

The smoke tests capture serial logs and assert stable boot markers for both boot paths, including ACPI `MADT`/`MCFG` discovery, PCIe enumeration, `virtio-blk` read success, and the existing userland markers. CI runs both tests on every push and pull request.

## Local CI With `act`

The main CI job is still:

```sh
act -j build-and-smoke
```

This repo includes a checked-in `.actrc`, and local `act` runs intentionally use `ubuntu-24.04=-self-hosted`. That means the job reuses your host toolchain instead of trying to reproduce a GitHub runner image inside Docker.

For local `act`, the host must provide:

- `cmake`
- `ninja`
- `nasm`
- `cpio`
- `xorriso`
- `qemu-system-x86_64`
- `x86_64-elf-gcc`
- `x86_64-elf-g++`
- `x86_64-elf-ld`
- discoverable OVMF firmware files for the default UEFI smoke path

## Artifacts

The build writes outputs under `build/artifacts/`:

- `boot.bin` — 512-byte BIOS MBR boot sector
- `kernel16.bin` — BIOS stage-1 loader with E820, EDD reads, long-mode entry, and ELF expansion
- `kernel_bios.elf` — shared low-half kernel core used by the BIOS path and loaded as a module by the Limine path
- `kernel_limine.elf` — higher-half Limine frontend that normalizes boot state and enters the shared kernel
- `cpustart.bin` — AP trampoline blob used for debugging / disassembly
- `initrd.cpio` — `cpio newc` initrd archive containing `/bin/init`, `/bin/yield`, and `/bin/fault`
- `virtio-test-disk.raw` — generated raw disk image used to validate the `virtio-blk` path during boot and smoke tests
- `user/*.elf` — statically linked user-space ELF inputs used to build the initrd
- `os1.iso` — default UEFI-only Limine ISO
- `os1.raw` — legacy BIOS raw disk image
- `os1.log` — serial log from the last UEFI `run`
- `os1-bios.log` — serial log from the last BIOS `run_bios`
- `smoke.log` / `smoke-bios.log` — captured smoke-test serial logs
- `dump.asm` / `cpustart.asm` — disassembly outputs

The helper wrapper scripts remain available as thin CMake frontends:

- `./run.sh`
- `./start.sh`
- `./dasm.sh`

## Documentation

- [Goals](GOALS.md) — project direction and design principles
- [Architecture](doc/ARCHITECTURE.md) — current-state source of truth for boot, memory, console, process, and test architecture
- [Review 2026-04-19](doc/2026-04-19-review.md) — historical review
- [Review 2026-04-21](doc/2026-04-21-review.md) — historical review
- [Milestone 1 Design: Boot Contract And Kernel Stabilization](doc/2026-04-22-milestone-1-boot-contract-and-kernel-stabilization.md) — implemented
- [Milestone 2 Design: Process Model And Isolation](doc/2026-04-22-milestone-2-process-model-and-isolation.md) — implemented
- [Milestone 3 Design: Modern Default Boot Path](doc/2026-04-22-milestone-3-modern-default-boot-path.md) — implemented
- [Milestone 4 Design: Modern Platform Support](doc/2026-04-22-milestone-4-modern-platform-support.md) — implemented
