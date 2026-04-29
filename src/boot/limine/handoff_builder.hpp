#pragma once

#include <stdint.h>

#include "arch/x86_64/cpu/cpu.hpp"
#include "handoff/boot_info.hpp"
#include "limine.h"

namespace limine_shim
{
struct LimineBootResponses
{
    const limine_bootloader_info_response* bootloader_info;
    const limine_executable_cmdline_response* cmdline;
    const limine_memmap_response* memory_map;
    const limine_rsdp_response* rsdp;
    const limine_smbios_response* smbios;
    const limine_framebuffer_response* framebuffer;
};

[[nodiscard]] bool prepare_kernel_handoff(uint64_t kernel_physical_end,
                                          cpu*& cpu_boot,
                                          uint64_t& boot_info_storage_physical);
[[nodiscard]] BootInfo* build_boot_info(const LimineBootResponses& responses,
                                        const limine_file& initrd_module,
                                        uint64_t kernel_physical_start,
                                        uint64_t kernel_physical_end,
                                        uint64_t boot_info_storage_physical);
}  // namespace limine_shim