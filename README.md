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
