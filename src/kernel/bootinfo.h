#ifndef _BOOTINFO_H_
#define _BOOTINFO_H_

#include <stddef.h>
#include <stdint.h>
#include <span>

// `BootInfo` is the stable bootloader-to-kernel contract. The BIOS loader
// populates it directly today, and later boot paths should normalize into the
// same layout instead of teaching the kernel multiple handoff ABIs.

constexpr uint64_t kBootInfoMagic = 0x4F5331424F4F5431ull; // "OS1BOOT1"
constexpr uint32_t kBootInfoVersion = 1;

constexpr size_t kBootInfoMaxMemoryRegions = 128;
constexpr size_t kBootInfoMaxModules = 16;
constexpr size_t kBootInfoMaxBootloaderNameBytes = 64;
constexpr size_t kBootInfoMaxCommandLineBytes = 256;
constexpr size_t kBootInfoMaxModuleNameBytes = 64;

enum class BootSource : uint32_t
{
	Unknown = 0,
	BiosLegacy = 1,
	Limine = 2,
	TestHarness = 3,
};

enum class BootFramebufferPixelFormat : uint16_t
{
	Unknown = 0,
	Rgb = 1,
	Bgr = 2,
};

// The first entries intentionally match BIOS E820 memory-type values so the
// early loader can forward them without translation.
enum class BootMemoryType : uint32_t
{
	Usable = 1,
	Reserved = 2,
	AcpiReclaimable = 3,
	AcpiNvs = 4,
	BadMemory = 5,
	Mmio = 6,
	Framebuffer = 7,
	KernelImage = 8,
	BootloaderReclaimable = 9,
};

#pragma pack(push, 1)

struct BootMemoryRegion
{
	uint64_t physical_start;
	uint64_t length;
	BootMemoryType type;
	uint32_t attributes;
};

struct BootTextConsoleInfo
{
	uint16_t columns;
	uint16_t rows;
	uint16_t cursor_x;
	uint16_t cursor_y;
};

struct BootFramebufferInfo
{
	uint64_t physical_address;
	uint32_t width;
	uint32_t height;
	uint32_t pitch_bytes;
	uint16_t bits_per_pixel;
	BootFramebufferPixelFormat pixel_format;
};

struct BootModuleInfo
{
	uint64_t physical_start;
	uint64_t length;
	const char *name;
};

struct BootInfo
{
	uint64_t magic;
	uint32_t version;
	BootSource source;

	uint64_t kernel_physical_start;
	uint64_t kernel_physical_end;
	uint64_t rsdp_physical;
	uint64_t smbios_physical;

	const char *command_line;
	const char *bootloader_name;

	BootTextConsoleInfo text_console;
	BootFramebufferInfo framebuffer;

	const BootMemoryRegion *memory_map;
	uint32_t memory_map_count;
	uint32_t reserved0;

	const BootModuleInfo *modules;
	uint32_t module_count;
	uint32_t reserved1;
};

#pragma pack(pop)

#define BOOTINFO_STATIC_ASSERT(name, expr) typedef char bootinfo_static_assert_##name[(expr) ? 1 : -1]

BOOTINFO_STATIC_ASSERT(memory_region_size, sizeof(BootMemoryRegion) == 24);
BOOTINFO_STATIC_ASSERT(text_console_size, sizeof(BootTextConsoleInfo) == 8);
BOOTINFO_STATIC_ASSERT(framebuffer_size, sizeof(BootFramebufferInfo) == 24);
BOOTINFO_STATIC_ASSERT(module_size, sizeof(BootModuleInfo) == 24);
BOOTINFO_STATIC_ASSERT(boot_info_size, sizeof(BootInfo) == 128);
BOOTINFO_STATIC_ASSERT(boot_info_magic_offset, offsetof(BootInfo, magic) == 0);
BOOTINFO_STATIC_ASSERT(boot_info_version_offset, offsetof(BootInfo, version) == 8);
BOOTINFO_STATIC_ASSERT(boot_info_source_offset, offsetof(BootInfo, source) == 12);
BOOTINFO_STATIC_ASSERT(boot_info_kernel_start_offset, offsetof(BootInfo, kernel_physical_start) == 16);
BOOTINFO_STATIC_ASSERT(boot_info_kernel_end_offset, offsetof(BootInfo, kernel_physical_end) == 24);
BOOTINFO_STATIC_ASSERT(boot_info_rsdp_offset, offsetof(BootInfo, rsdp_physical) == 32);
BOOTINFO_STATIC_ASSERT(boot_info_smbios_offset, offsetof(BootInfo, smbios_physical) == 40);
BOOTINFO_STATIC_ASSERT(boot_info_command_line_offset, offsetof(BootInfo, command_line) == 48);
BOOTINFO_STATIC_ASSERT(boot_info_bootloader_name_offset, offsetof(BootInfo, bootloader_name) == 56);
BOOTINFO_STATIC_ASSERT(boot_info_text_console_offset, offsetof(BootInfo, text_console) == 64);
BOOTINFO_STATIC_ASSERT(boot_info_framebuffer_offset, offsetof(BootInfo, framebuffer) == 72);
BOOTINFO_STATIC_ASSERT(boot_info_memory_map_offset, offsetof(BootInfo, memory_map) == 96);
BOOTINFO_STATIC_ASSERT(boot_info_memory_map_count_offset, offsetof(BootInfo, memory_map_count) == 104);
BOOTINFO_STATIC_ASSERT(boot_info_modules_offset, offsetof(BootInfo, modules) == 112);
BOOTINFO_STATIC_ASSERT(boot_info_module_count_offset, offsetof(BootInfo, module_count) == 120);

#undef BOOTINFO_STATIC_ASSERT

[[nodiscard]] inline bool BootMemoryRegionIsUsable(const BootMemoryRegion &region)
{
	return region.type == BootMemoryType::Usable;
}

[[nodiscard]] inline const char *BootSourceName(BootSource source)
{
	switch(source)
	{
	case BootSource::BiosLegacy: return "bios";
	case BootSource::Limine: return "limine";
	case BootSource::TestHarness: return "test";
	default: return "unknown";
	}
}

[[nodiscard]] inline const char *BootFramebufferPixelFormatName(BootFramebufferPixelFormat pixel_format)
{
	switch(pixel_format)
	{
	case BootFramebufferPixelFormat::Rgb: return "rgb";
	case BootFramebufferPixelFormat::Bgr: return "bgr";
	default: return "unknown";
	}
}

[[nodiscard]] inline std::span<const BootMemoryRegion>
BootMemoryRegions(const BootInfo &info)
{
	return std::span<const BootMemoryRegion>(info.memory_map, info.memory_map_count);
}

[[nodiscard]] const BootInfo *OwnBootInfo(const BootInfo *source);

#endif
