# Host Test Support

This directory contains hosted shims used only by `tests/host`.

- `physical_memory.*` maps synthetic physical addresses to host memory so page-frame and page-table tests can exercise production code without mapping low addresses.
- `debug_stub.cpp` satisfies the kernel debug logger interface without touching serial ports.
- `memory_stubs.cpp` supplies the non-libc word-fill symbols used by kernel memory-management code.

Production kernel code must not include GoogleTest or depend on these support files.
