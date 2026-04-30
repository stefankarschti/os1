// Generated kernel boot-envelope constants shared by BIOS assembly, the Limine
// shim, and the kernel handoff code.
#pragma once

#include <stdint.h>

constexpr uint64_t kKernelImageLoadAddress = 0x10000;
constexpr uint64_t kInitrdLoadAddress = 0x90000;
constexpr uint64_t kKernelReservedPhysicalStart = 0x100000;
constexpr uint64_t kKernelReservedPhysicalBytes = 524288;
constexpr uint64_t kKernelReservedPhysicalEnd = 0x180000;
constexpr uint64_t kKernelPostImageReserveBytes = 0x3000;
constexpr uint64_t kKernelVirtualOffset = 0xFFFFFFFF80000000;
constexpr uint64_t kKernelShimVirtualBase = 0xFFFFFFFF90000000;
constexpr uint64_t kDirectMapBase = 0xFFFF800000000000;
