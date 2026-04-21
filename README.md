# os1

Operating System Playground.

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

## Documentation

- [Architecture](doc/ARCHITECTURE.md)
