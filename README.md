# os1

`os1` is a self-documented teaching and engineering operating system project: a small, technically serious `x86_64` OS built for clarity, runnable vertical slices, and modern OS concepts rather than feature sprawl. The near-term focus is a terminal-first, QEMU/virtio-first system with a clean boot handoff and protected userland, growing later toward SMP, networking, remote administration, and an optional framebuffer desktop layer without losing architectural coherence.

## Prerequisites

This project expects a freestanding cross-toolchain and NASM:

- `x86_64-elf-gcc`
- `x86_64-elf-g++`
- `x86_64-elf-ld`
- `nasm`

Optional helper targets also use:

- `qemu-system-x86_64` for `run`
- `x86_64-elf-objdump` or `objdump` for `disasm`

On macOS with Homebrew, the core setup is:

```sh
brew install x86_64-elf-gcc nasm qemu
```

## Build

If you have an old Make-based `build/` directory, remove it once before switching:

```sh
rm -rf build
```

Configure and build with CMake:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-elf.cmake
cmake --build build
```

The default build produces the raw disk image at `build/artifacts/os1.raw`.

In VS Code with the CMake Tools extension, this repo now ships a `default` configure preset and matching build presets. Open the folder, let CMake Tools use presets, then:

- configure with the `default` preset
- build with the `default` preset
- run QEMU by selecting the `run` build preset or building the `run` target
- generate disassembly with the `disasm` build preset or target

If CMake Tools was previously configured with a different generator in the same `build/` directory, use `CMake: Delete Cache and Reconfigure` once so it can switch cleanly to the preset-driven Ninja build.

## Run and Disassemble

Run the image in QEMU:

```sh
cmake --build build --target run
```

Generate disassembly outputs:

```sh
cmake --build build --target disasm
```

Artifacts are written under `build/artifacts/`:

- `boot.bin`
- `kernel16.bin`
- `kernel64.elf`
- `cpustart.bin`
- `os1.raw`
- `os1.log`
- `dump.asm`
- `cpustart.asm`

The existing helper scripts remain available as wrappers around the CMake workflow:

- `./run.sh`
- `./start.sh`
- `./dasm.sh`

## Local CI With `act`

This repo includes a checked-in `.actrc`, so the main CI job can be exercised locally with:

```sh
act -j build-and-smoke
```

For local `act` runs, `ubuntu-24.04` is mapped to `-self-hosted` on purpose. That avoids `act`'s missing `ubuntu-24.04` container mapping and lets the job reuse the local toolchain you already need for normal development. The `act` path therefore validates these host tools instead of provisioning them:

- `cmake`
- `ninja`
- `nasm`
- `qemu-system-x86_64`
- `x86_64-elf-gcc`
- `x86_64-elf-g++`
- `x86_64-elf-ld`

## Documentation

- [Goals](GOALS.md)
- [Architecture](doc/ARCHITECTURE.md)
- [Review 2026-04-21](doc/2026-04-21-review.md)
- [Milestone 1 Design: Boot Contract And Kernel Stabilization](doc/2026-04-22-milestone-1-boot-contract-and-kernel-stabilization.md)
- [Milestone 2 Design: Process Model And Isolation](doc/2026-04-22-milestone-2-process-model-and-isolation.md)
- [Milestone 3 Design: Modern Default Boot Path](doc/2026-04-22-milestone-3-modern-default-boot-path.md)
- [Milestone 4 Design: Modern Platform Support](doc/2026-04-22-milestone-4-modern-platform-support.md)
