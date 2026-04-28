// Fixed early physical layout used by BIOS assembly, Limine handoff code, and
// kernel C++ before the page-frame allocator owns memory lifetime.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel_layout.hpp"  // IWYU pragma: export

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
// BIOS disk packets live in dedicated low-memory scratch so firmware I/O does
// not scribble over executable boot sectors while loading later stages.
constexpr uint64_t kBootDiskPacketAddress = 0x0800;
constexpr uint64_t kLoaderDiskPacketAddress = 0x0820;
constexpr uint64_t kLoaderDiskRangeStateAddress = 0x0840;
constexpr uint64_t kBootMemoryRegionBufferAddress = 0x6000;
constexpr uint64_t kBootModuleInfoBufferAddress = 0x7000;
// The Limine shim mirrors the BIOS handoff layout by copying strings into a
// small low-memory pool before switching to its final identity-mapped CR3.
constexpr uint64_t kBootStringBufferAddress = 0x7200;
constexpr uint64_t kBootStringBufferSizeBytes = 0x0E00;
constexpr size_t kBootMemoryRegionCapacity = 128;

// The temporary real-mode page tables only exist long enough to enter long
// mode. The kernel later replaces them with its own page tables.
constexpr uint64_t kEarlyLongModePageTablesAddress = 0xA000;

// The BIOS loader expands the kernel ELF from this low-memory image buffer.
// The generated kernel_layout.hpp contract keeps this staging address aligned
// with the BIOS raw-image slot and the initrd staging buffer.

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
constexpr uint64_t kPageFrameBitmapQwordLimit = kPageFrameBitmapSizeBytes / sizeof(uint64_t);

// Early low-memory scratch remains reserved until boot no longer depends on it.
constexpr uint64_t kEarlyReservedPhysicalEnd = 0x20000;


constexpr uint64_t kKernelPml4Index = (kKernelVirtualOffset >> 39) & 0x1FFull;
constexpr uint64_t kDirectMapPml4Index = (kDirectMapBase >> 39) & 0x1FFull;

[[nodiscard]] constexpr uint64_t phys_to_virt(uint64_t physical_address)
{
	return physical_address + kDirectMapBase;
}

[[nodiscard]] constexpr uint64_t virt_to_phys(uint64_t virtual_address)
{
	if(virtual_address >= kDirectMapBase)
	{
		return virtual_address - kDirectMapBase;
	}
	return virtual_address - kKernelVirtualOffset;
}

extern bool g_kernel_direct_map_ready;

template<typename T>
[[nodiscard]] inline T* kernel_physical_pointer(uint64_t physical_address)
{
	const uint64_t virtual_address =
		g_kernel_direct_map_ready ? phys_to_virt(physical_address) : physical_address;
	return reinterpret_cast<T*>(virtual_address);
}


// User mappings stay in their own PML4 slot even after the higher-half
// migration; the kernel now reaches physical memory through the direct map and
// keeps only narrow low bootstrap identity exceptions.
constexpr uint64_t kUserPml4Index = 1;
constexpr uint64_t kUserSpaceBase = 0x0000008000000000ull;
constexpr uint64_t kUserImageBase = 0x0000008000400000ull;
constexpr uint64_t kUserStackTop = 0x0000008040000000ull;
constexpr size_t kUserStackPages = 16;
constexpr size_t kKernelThreadStackPages = 4;

constexpr uint16_t kBootTextColumns = 80;
constexpr uint16_t kBootTextRows = 25;
