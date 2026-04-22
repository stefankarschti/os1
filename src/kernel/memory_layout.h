#ifndef _MEMORY_LAYOUT_H_
#define _MEMORY_LAYOUT_H_

#include <stdint.h>
#include <stddef.h>

// These fixed addresses are still part of the early BIOS-era bring-up path.
// Milestone 1 names them explicitly so ownership is clear even before later
// milestones make more of them dynamic.

constexpr uint64_t kPageSize = 0x1000;

// Loader16 and the AP trampoline intentionally reuse the same physical page in
// different phases of boot. The lifetimes do not overlap.
constexpr uint64_t kLoader16LoadAddress = 0x1000;
constexpr uint64_t kApTrampolineAddress = kLoader16LoadAddress;

// The loader publishes the BootInfo block in low memory so the kernel can copy
// it before any allocator or paging policy changes ownership.
constexpr uint64_t kBootInfoAddress = 0x0500;
constexpr uint64_t kBootMemoryRegionBufferAddress = 0x6000;
constexpr size_t kBootMemoryRegionCapacity = 128;

// The temporary real-mode page tables only exist long enough to enter long
// mode. The kernel later replaces them with its own page tables.
constexpr uint64_t kEarlyLongModePageTablesAddress = 0xA000;

// The BIOS loader expands the kernel ELF from this low-memory image buffer.
constexpr uint64_t kKernelImageLoadAddress = 0x10000;

// The AP trampoline communicates through a tiny parameter block in low memory
// because secondary CPUs start before they have per-CPU stacks or rich state.
constexpr uint64_t kApStartupCpuPageAddress = 0x20;
constexpr uint64_t kApStartupRipAddress = 0x28;
constexpr uint64_t kApStartupCr3Address = 0x30;
constexpr uint64_t kApStartupIdtAddress = 0x38;
constexpr uint64_t kApStartupIdtSizeBytes = 6;

// The page-frame bitmap still lives in fixed low memory in M1 so the kernel
// can bring up allocation deterministically before later cleanup work.
constexpr uint64_t kPageFrameBitmapBaseAddress = 0x20000;
constexpr uint64_t kPageFrameBitmapSizeBytes = 0x40000;
constexpr uint64_t kPageFrameBitmapQwordLimit =
		kPageFrameBitmapSizeBytes / sizeof(uint64_t);

// Early low-memory scratch remains reserved until boot no longer depends on it.
constexpr uint64_t kEarlyReservedPhysicalEnd = 0x20000;
constexpr uint64_t kKernelReservedPhysicalStart = 0x100000;
constexpr uint64_t kKernelReservedPhysicalEnd = 0x160000;

constexpr uint16_t kBootTextColumns = 80;
constexpr uint16_t kBootTextRows = 25;

#endif
