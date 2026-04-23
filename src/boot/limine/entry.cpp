#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"
#include "cpu.h"
#include "limine.h"
#include "memory_layout.h"
#include "memory.h"

// The shared kernel still expects to start on a low identity-mapped stack.
// Keep this as a real assembler symbol rather than a naked C++ function:
// GCC's naked-function semantics are compiler-sensitive, and the GitHub runner
// uses a newer cross compiler than local `act` runs.
extern "C" [[noreturn]] void limine_enter_kernel(void (*)(BootInfo *, cpu *), BootInfo *, cpu *);

asm(R"ASM(
.global limine_enter_kernel
.type limine_enter_kernel, @function
limine_enter_kernel:
	cli
	cld
	mov %rdi, %rax
	lea 4096(%rdx), %rsp
	and $-16, %rsp
	mov %rsi, %rdi
	mov %rdx, %rsi
	call *%rax
1:
	cli
	hlt
	jmp 1b
.size limine_enter_kernel, .-limine_enter_kernel
)ASM");

namespace
{
constexpr uint64_t kPageSize = 0x1000;
constexpr uint64_t kPageMask = ~(kPageSize - 1);
constexpr uint64_t kTwoMiBPageSize = 0x200000;
constexpr uint64_t kMaxIdentityMapBytes = 512 * kTwoMiBPageSize;
constexpr uint64_t kHugePageBit = 1ull << 7;
constexpr uint64_t kPageEntryAddressMask = 0x000FFFFFFFFFF000ull;
constexpr uint64_t kOneGiBPageAddressMask = 0x000FFFFFC0000000ull;
constexpr uint64_t kTwoMiBPageAddressMask = 0x000FFFFFFFE00000ull;
constexpr uint64_t kLimineBaseRevisionRequested = 6;
constexpr uint32_t kElfMagic = 0x464C457Fu;
constexpr uint32_t kElfProgramTypeLoad = 1;
constexpr const char *kKernelModuleName = "kernel_bios.elf";
constexpr const char *kInitrdModuleName = "initrd.cpio";

struct Elf64Header
{
	uint32_t magic;
	uint8_t ident[12];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint64_t entry;
	uint64_t phoff;
	uint64_t shoff;
	uint32_t flags;
	uint16_t ehsize;
	uint16_t phentsize;
	uint16_t phnum;
	uint16_t shentsize;
	uint16_t shnum;
	uint16_t shstrndx;
} __attribute__((packed));

struct Elf64ProgramHeader
{
	uint32_t type;
	uint32_t flags;
	uint64_t offset;
	uint64_t vaddr;
	uint64_t paddr;
	uint64_t filesz;
	uint64_t memsz;
	uint64_t align;
} __attribute__((packed));

struct BootStringArena
{
	char *base;
	size_t capacity;
	size_t used;
};

struct LowHandoffBootInfoStorage
{
	BootInfo info{};
	BootMemoryRegion memory_map[kBootInfoMaxMemoryRegions]{};
	BootModuleInfo modules[kBootInfoMaxModules]{};
	char string_pool[
		kBootInfoMaxBootloaderNameBytes
		+ kBootInfoMaxCommandLineBytes
		+ (kBootInfoMaxModules * kBootInfoMaxModuleNameBytes)
	]{};
};

alignas(kPageSize) constinit uint64_t g_low_identity_pml3[512]{};
alignas(kPageSize) constinit uint64_t g_low_identity_pml2[512]{};

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t g_limine_requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t g_limine_base_revision[3] = LIMINE_BASE_REVISION(kLimineBaseRevisionRequested);

__attribute__((used, section(".limine_requests")))
volatile limine_hhdm_request g_hhdm_request = {
	.id = LIMINE_HHDM_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_memmap_request g_memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request g_framebuffer_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_bootloader_info_request g_bootloader_info_request = {
	.id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_executable_cmdline_request g_cmdline_request = {
	.id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_module_request g_module_request = {
	.id = LIMINE_MODULE_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
	.internal_module_count = 0,
	.internal_modules = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_rsdp_request g_rsdp_request = {
	.id = LIMINE_RSDP_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests")))
volatile limine_smbios_request g_smbios_request = {
	.id = LIMINE_SMBIOS_REQUEST_ID,
	.revision = 0,
	.response = nullptr,
};

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t g_limine_requests_end[2] = LIMINE_REQUESTS_END_MARKER;

[[noreturn]] void HaltForever()
{
	for(;;)
	{
		asm volatile("cli");
		asm volatile("hlt");
	}
}

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] uint64_t ReadCr3()
{
	uint64_t value = 0;
	asm volatile("mov %%cr3, %0" : "=r"(value));
	return value;
}

[[nodiscard]] size_t StringLength(const char *text)
{
	size_t length = 0;
	if(nullptr == text)
	{
		return 0;
	}
	while(text[length])
	{
		++length;
	}
	return length;
}

void InitSerial()
{
	constexpr uint16_t kSerialPort = 0x3F8;
	outb(kSerialPort + 1, 0x00);
	outb(kSerialPort + 3, 0x80);
	outb(kSerialPort + 0, 0x01);
	outb(kSerialPort + 1, 0x00);
	outb(kSerialPort + 3, 0x03);
	outb(kSerialPort + 2, 0xC7);
	outb(kSerialPort + 4, 0x0B);
}

void WriteSerialChar(char value)
{
	constexpr uint16_t kSerialPort = 0x3F8;
	while(0 == (inb(kSerialPort + 5) & 0x20))
	{
	}
	outb(kSerialPort, (uint8_t)value);
}

void WriteSerial(const char *text)
{
	if(nullptr == text)
	{
		return;
	}
	while(*text)
	{
		WriteSerialChar(*text++);
	}
}

void WriteSerialLn(const char *text)
{
	WriteSerial(text);
	WriteSerial("\n");
}

void WriteSerialHex(uint64_t value)
{
	static const char hexdigits[] = "0123456789ABCDEF";
	for(int nibble = 15; nibble >= 0; --nibble)
	{
		const uint8_t digit = (value >> (nibble * 4)) & 0xFu;
		WriteSerialChar(hexdigits[digit]);
	}
}

void CopyBytes(void *destination, const void *source, size_t length)
{
	uint8_t *dest = (uint8_t*)destination;
	const uint8_t *src = (const uint8_t*)source;
	for(size_t i = 0; i < length; ++i)
	{
		dest[i] = src[i];
	}
}

void ZeroBytes(void *destination, size_t length)
{
	uint8_t *dest = (uint8_t*)destination;
	for(size_t i = 0; i < length; ++i)
	{
		dest[i] = 0;
	}
}

void CopyString(char *destination, size_t capacity, const char *source)
{
	if((nullptr == destination) || (0 == capacity))
	{
		return;
	}

	destination[0] = 0;
	if(nullptr == source)
	{
		return;
	}

	const size_t source_length = StringLength(source);
	const size_t copy_length = (source_length < (capacity - 1))
		? source_length
		: (capacity - 1);
	CopyBytes(destination, source, copy_length);
	destination[copy_length] = 0;
}

void AppendString(char *destination, size_t capacity, const char *suffix)
{
	if((nullptr == destination) || (nullptr == suffix) || (0 == capacity))
	{
		return;
	}

	size_t destination_length = StringLength(destination);
	while((destination_length + 1) < capacity && *suffix)
	{
		destination[destination_length++] = *suffix++;
	}
	destination[destination_length] = 0;
}

[[nodiscard]] bool StringsEqual(const char *left, const char *right)
{
	if((nullptr == left) || (nullptr == right))
	{
		return false;
	}

	size_t index = 0;
	while(left[index] && right[index])
	{
		if(left[index] != right[index])
		{
			return false;
		}
		++index;
	}

	return left[index] == right[index];
}

[[nodiscard]] bool PathEndsWith(const char *path, const char *suffix)
{
	if((nullptr == path) || (nullptr == suffix))
	{
		return false;
	}

	const size_t path_length = StringLength(path);
	const size_t suffix_length = StringLength(suffix);
	if(path_length < suffix_length)
	{
		return false;
	}

	return StringsEqual(path + path_length - suffix_length, suffix);
}

[[nodiscard]] uint64_t PageIndex(uint64_t virtual_address, unsigned shift)
{
	return (virtual_address >> shift) & 0x1FFull;
}

[[nodiscard]] const uint64_t *MapPhysicalTable(uint64_t physical_address)
{
	if(nullptr == g_hhdm_request.response)
	{
		return nullptr;
	}
	return (const uint64_t*)(physical_address + g_hhdm_request.response->offset);
}

template <typename T>
[[nodiscard]] T *MapPhysicalPointer(uint64_t physical_address)
{
	if(nullptr == g_hhdm_request.response)
	{
		return nullptr;
	}
	return (T*)(physical_address + g_hhdm_request.response->offset);
}

[[nodiscard]] bool TranslateLimineVirtual(uint64_t virtual_address, uint64_t &physical_address)
{
	const uint64_t *pml4 = MapPhysicalTable(ReadCr3() & kPageMask);
	if(nullptr == pml4)
	{
		return false;
	}

	const uint64_t pml4e = pml4[PageIndex(virtual_address, 39)];
	if(0 == (pml4e & 1ull))
	{
		return false;
	}

	const uint64_t *pml3 = MapPhysicalTable(pml4e & kPageEntryAddressMask);
	if(nullptr == pml3)
	{
		return false;
	}
	const uint64_t pml3e = pml3[PageIndex(virtual_address, 30)];
	if(0 == (pml3e & 1ull))
	{
		return false;
	}
	if(0 != (pml3e & kHugePageBit))
	{
		physical_address = (pml3e & kOneGiBPageAddressMask)
			| (virtual_address & ((1ull << 30) - 1ull));
		return true;
	}

	const uint64_t *pml2 = MapPhysicalTable(pml3e & kPageEntryAddressMask);
	if(nullptr == pml2)
	{
		return false;
	}
	const uint64_t pml2e = pml2[PageIndex(virtual_address, 21)];
	if(0 == (pml2e & 1ull))
	{
		return false;
	}
	if(0 != (pml2e & kHugePageBit))
	{
		physical_address = (pml2e & kTwoMiBPageAddressMask)
			| (virtual_address & ((1ull << 21) - 1ull));
		return true;
	}

	const uint64_t *pml1 = MapPhysicalTable(pml2e & kPageEntryAddressMask);
	if(nullptr == pml1)
	{
		return false;
	}
	const uint64_t pml1e = pml1[PageIndex(virtual_address, 12)];
	if(0 == (pml1e & 1ull))
	{
		return false;
	}

	physical_address = (pml1e & kPageEntryAddressMask) | (virtual_address & 0xFFFull);
	return true;
}

[[nodiscard]] bool TranslateShimPointer(const void *pointer, uint64_t &physical_address)
{
	return TranslateLimineVirtual((uint64_t)pointer, physical_address);
}

void ReloadCr3()
{
	const uint64_t value = ReadCr3();
	asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

[[nodiscard]] bool EnsureLowIdentityWindow(uint64_t required_bytes)
{
	const uint64_t mapped_bytes = AlignUp(required_bytes, kTwoMiBPageSize);
	if((0 == mapped_bytes) || (mapped_bytes > kMaxIdentityMapBytes))
	{
		return false;
	}

	uint64_t pml4_physical = ReadCr3() & kPageMask;
	uint64_t *pml4 = MapPhysicalPointer<uint64_t>(pml4_physical);
	if(nullptr == pml4)
	{
		return false;
	}

	uint64_t pml3_physical = 0;
	uint64_t *pml3 = nullptr;
	if(0 == (pml4[0] & 1ull))
	{
		ZeroBytes(g_low_identity_pml3, sizeof(g_low_identity_pml3));
		if(!TranslateShimPointer(g_low_identity_pml3, pml3_physical))
		{
			return false;
		}
		pml4[0] = pml3_physical | 0x3ull;
		pml3 = g_low_identity_pml3;
	}
	else
	{
		pml3_physical = pml4[0] & kPageEntryAddressMask;
		pml3 = MapPhysicalPointer<uint64_t>(pml3_physical);
		if(nullptr == pml3)
		{
			return false;
		}
	}

	if(0 != (pml3[0] & kHugePageBit))
	{
		return true;
	}

	uint64_t pml2_physical = 0;
	uint64_t *pml2 = nullptr;
	if(0 == (pml3[0] & 1ull))
	{
		ZeroBytes(g_low_identity_pml2, sizeof(g_low_identity_pml2));
		if(!TranslateShimPointer(g_low_identity_pml2, pml2_physical))
		{
			return false;
		}
		pml3[0] = pml2_physical | 0x3ull;
		pml2 = g_low_identity_pml2;
	}
	else
	{
		pml2_physical = pml3[0] & kPageEntryAddressMask;
		pml2 = MapPhysicalPointer<uint64_t>(pml2_physical);
		if(nullptr == pml2)
		{
			return false;
		}
	}

	const uint64_t page_count = mapped_bytes / kTwoMiBPageSize;
	for(uint64_t i = 0; i < page_count; ++i)
	{
		if(0 == (pml2[i] & 1ull))
		{
			// The shared low-half kernel still expects the first boot-critical
			// physical window to be executable via identity addresses. The Limine
			// shim only maps the minimum range needed for that handoff.
			pml2[i] = (i * kTwoMiBPageSize) | 0x83ull;
		}
	}

	ReloadCr3();
	return true;
}

template <typename T>
[[nodiscard]] bool LiminePointerMapped(const T *pointer)
{
	if(nullptr == pointer)
	{
		return false;
	}

	uint64_t physical_address = 0;
	return TranslateShimPointer(pointer, physical_address);
}

[[nodiscard]] BootMemoryType TranslateMemoryType(uint64_t limine_type)
{
	switch(limine_type)
	{
	case LIMINE_MEMMAP_USABLE: return BootMemoryType::Usable;
	case LIMINE_MEMMAP_RESERVED: return BootMemoryType::Reserved;
	case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return BootMemoryType::AcpiReclaimable;
	case LIMINE_MEMMAP_ACPI_NVS: return BootMemoryType::AcpiNvs;
	case LIMINE_MEMMAP_BAD_MEMORY: return BootMemoryType::BadMemory;
	case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return BootMemoryType::BootloaderReclaimable;
	case LIMINE_MEMMAP_FRAMEBUFFER: return BootMemoryType::Framebuffer;
	case LIMINE_MEMMAP_RESERVED_MAPPED: return BootMemoryType::Mmio;
	case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return BootMemoryType::Reserved;
	default: return BootMemoryType::Reserved;
	}
}

[[nodiscard]] BootFramebufferPixelFormat DetectFramebufferPixelFormat(const limine_framebuffer *framebuffer)
{
	if((nullptr == framebuffer) || (LIMINE_FRAMEBUFFER_RGB != framebuffer->memory_model))
	{
		return BootFramebufferPixelFormat::Unknown;
	}

	if((8 != framebuffer->red_mask_size)
		|| (8 != framebuffer->green_mask_size)
		|| (8 != framebuffer->blue_mask_size))
	{
		return BootFramebufferPixelFormat::Unknown;
	}

	if((16 == framebuffer->red_mask_shift)
		&& (8 == framebuffer->green_mask_shift)
		&& (0 == framebuffer->blue_mask_shift))
	{
		return BootFramebufferPixelFormat::Rgb;
	}

	if((0 == framebuffer->red_mask_shift)
		&& (8 == framebuffer->green_mask_shift)
		&& (16 == framebuffer->blue_mask_shift))
	{
		return BootFramebufferPixelFormat::Bgr;
	}

	return BootFramebufferPixelFormat::Unknown;
}

[[nodiscard]] const limine_file *FindModule(const char *name)
{
	const auto *response = g_module_request.response;
	if(!LiminePointerMapped(response) || !LiminePointerMapped(response->modules))
	{
		return nullptr;
	}

	for(uint64_t i = 0; i < response->module_count; ++i)
	{
		const limine_file *module = response->modules[i];
		if(!LiminePointerMapped(module))
		{
			continue;
		}
		const char *path = LiminePointerMapped(module->path) ? module->path : nullptr;
		const char *string = LiminePointerMapped(module->string) ? module->string : nullptr;
		if((nullptr == path) && (nullptr == string))
		{
			continue;
		}

		if(PathEndsWith(path, name)
			|| StringsEqual(string, name)
			|| PathEndsWith(string, name))
		{
			return module;
		}
	}

	return nullptr;
}

[[nodiscard]] bool LoadKernelImage(const limine_file &kernel_file,
		uint64_t &entry_point,
		uint64_t &kernel_physical_start,
		uint64_t &kernel_physical_end,
		cpu *&cpu_boot,
		uint64_t &boot_info_storage_physical)
{
	if((nullptr == kernel_file.address) || (kernel_file.size < sizeof(Elf64Header)))
	{
		return false;
	}

	const auto *header = (const Elf64Header*)kernel_file.address;
	if((header->magic != kElfMagic) || (header->phentsize != sizeof(Elf64ProgramHeader)))
	{
		return false;
	}

	WriteSerial("[limine-shim] kernel module @ 0x");
	WriteSerialHex((uint64_t)kernel_file.address);
	WriteSerial(" size=0x");
	WriteSerialHex(kernel_file.size);
	WriteSerialLn("");

	kernel_physical_start = ~0ull;
	kernel_physical_end = 0;
	for(uint16_t i = 0; i < header->phnum; ++i)
	{
		const uint64_t program_offset = header->phoff + (uint64_t)i * header->phentsize;
		if((program_offset + sizeof(Elf64ProgramHeader)) > kernel_file.size)
		{
			return false;
		}

		const auto *program = (const Elf64ProgramHeader*)((const uint8_t*)kernel_file.address + program_offset);
		if(kElfProgramTypeLoad != program->type)
		{
			continue;
		}
		if((program->memsz < program->filesz)
			|| ((program->offset + program->filesz) > kernel_file.size))
		{
			return false;
		}

		WriteSerial("[limine-shim] PT_LOAD vaddr=0x");
		WriteSerialHex(program->vaddr);
		WriteSerial(" filesz=0x");
		WriteSerialHex(program->filesz);
		WriteSerial(" memsz=0x");
		WriteSerialHex(program->memsz);
		WriteSerialLn("");

		// Limine module pointers are virtual addresses in the bootloader's page
		// tables. The shared kernel image is still low-linked, so the shim writes
		// it by physical address through the HHDM mapping instead of assuming an
		// identity map already exists.
		uint8_t *segment = MapPhysicalPointer<uint8_t>(program->vaddr);
		if(nullptr == segment)
		{
			return false;
		}
		ZeroBytes(segment, (size_t)program->memsz);
		CopyBytes(segment,
				(const uint8_t*)kernel_file.address + program->offset,
				(size_t)program->filesz);

		if(program->vaddr < kernel_physical_start)
		{
			kernel_physical_start = program->vaddr;
		}
		if((program->vaddr + program->memsz) > kernel_physical_end)
		{
			kernel_physical_end = program->vaddr + program->memsz;
		}
	}

	WriteSerial("[limine-shim] loaded range 0x");
	WriteSerialHex(kernel_physical_start);
	WriteSerial("-0x");
	WriteSerialHex(kernel_physical_end);
	WriteSerialLn("");

	if((~0ull == kernel_physical_start) || (kernel_physical_end <= kernel_physical_start))
	{
		return false;
	}

	cpu_boot = (cpu*)AlignUp(kernel_physical_end, kPageSize);
	uint8_t *cpu_boot_page = MapPhysicalPointer<uint8_t>((uint64_t)cpu_boot);
	if(nullptr == cpu_boot_page)
	{
		return false;
	}
	ZeroBytes(cpu_boot_page, kPageSize);
	boot_info_storage_physical = AlignUp((uint64_t)cpu_boot + kPageSize, kPageSize);
	const uint64_t boot_info_storage_end = AlignUp(
			boot_info_storage_physical + sizeof(LowHandoffBootInfoStorage),
			kPageSize);
	if(boot_info_storage_end > kKernelReservedPhysicalEnd)
	{
		return false;
	}
	const uint64_t low_handoff_end = AlignUp((uint64_t)cpu_boot + kPageSize, kPageSize);
	if(!EnsureLowIdentityWindow(low_handoff_end))
	{
		return false;
	}
	entry_point = header->entry;
	return true;
}

[[nodiscard]] LowHandoffBootInfoStorage *MapBootInfoStorage(uint64_t physical_address)
{
	return MapPhysicalPointer<LowHandoffBootInfoStorage>(physical_address);
}

[[nodiscard]] BootInfo *BootInfoStorage(LowHandoffBootInfoStorage *storage)
{
	return storage ? &storage->info : nullptr;
}

[[nodiscard]] BootMemoryRegion *BootMemoryMapStorage(LowHandoffBootInfoStorage *storage)
{
	return storage ? storage->memory_map : nullptr;
}

[[nodiscard]] BootModuleInfo *BootModuleStorage(LowHandoffBootInfoStorage *storage)
{
	return storage ? storage->modules : nullptr;
}

[[nodiscard]] BootStringArena CreateBootStringArena(LowHandoffBootInfoStorage *storage)
{
	BootStringArena arena{};
	if(nullptr == storage)
	{
		return arena;
	}
	arena.base = storage->string_pool;
	arena.capacity = sizeof(storage->string_pool);
	arena.used = 0;
	return arena;
}

[[nodiscard]] char *ReserveBootString(BootStringArena &arena,
		size_t capacity)
{
	if((nullptr == arena.base) || (0 == capacity))
	{
		return nullptr;
	}
	if((arena.used + capacity) > arena.capacity)
	{
		return nullptr;
	}

	char *storage = arena.base + arena.used;
	ZeroBytes(storage, capacity);
	arena.used += capacity;
	return storage;
}

[[nodiscard]] const char *CopyBootString(BootStringArena &arena,
		size_t capacity,
		const char *source)
{
	char *storage = ReserveBootString(arena, capacity);
	if(nullptr == storage)
	{
		return nullptr;
	}
	CopyString(storage, capacity, source);
	return storage;
}

[[nodiscard]] bool PopulateBootloaderInfo(BootInfo &boot_info, BootStringArena &arena)
{
	const auto *response = g_bootloader_info_request.response;
	const char *name = "limine";
	const char *version = nullptr;
	if(LiminePointerMapped(response))
	{
		if(LiminePointerMapped(response->name))
		{
			name = response->name;
		}
		if(LiminePointerMapped(response->version))
		{
			version = response->version;
		}
	}

	char *storage = ReserveBootString(arena, kBootInfoMaxBootloaderNameBytes);
	if(nullptr == storage)
	{
		return false;
	}
	CopyString(storage, kBootInfoMaxBootloaderNameBytes, name);
	if((nullptr != version) && version[0])
	{
		AppendString(storage, kBootInfoMaxBootloaderNameBytes, " ");
		AppendString(storage, kBootInfoMaxBootloaderNameBytes, version);
	}
	boot_info.bootloader_name = storage;
	return true;
}

bool PopulateCommandLine(BootInfo &boot_info, BootStringArena &arena)
{
	const auto *response = g_cmdline_request.response;
	if(!LiminePointerMapped(response)
		|| !LiminePointerMapped(response->cmdline)
		|| (0 == response->cmdline[0]))
	{
		return true;
	}

	boot_info.command_line = CopyBootString(arena,
			kBootInfoMaxCommandLineBytes,
			response->cmdline);
	return nullptr != boot_info.command_line;
}

[[nodiscard]] bool PopulateMemoryMap(BootInfo &boot_info, BootMemoryRegion *memory_map)
{
	const auto *response = g_memmap_request.response;
	if((nullptr == memory_map)
		|| !LiminePointerMapped(response)
		|| (response->entry_count > kBootInfoMaxMemoryRegions))
	{
		return false;
	}
	if(!LiminePointerMapped(response->entries))
	{
		return false;
	}

	for(uint64_t i = 0; i < response->entry_count; ++i)
	{
		const auto *entry = response->entries[i];
		if(!LiminePointerMapped(entry))
		{
			return false;
		}

		memory_map[i].physical_start = entry->base;
		memory_map[i].length = entry->length;
		memory_map[i].type = TranslateMemoryType(entry->type);
		memory_map[i].attributes = 0;
	}

	boot_info.memory_map = memory_map;
	boot_info.memory_map_count = (uint32_t)response->entry_count;
	return true;
}

[[nodiscard]] bool PopulateInitrdModule(BootInfo &boot_info,
		BootStringArena &arena,
		const limine_file &initrd_module,
		BootModuleInfo *modules)
{
	if(nullptr == modules)
	{
		return false;
	}

	uint64_t initrd_physical = 0;
	if(!TranslateLimineVirtual((uint64_t)initrd_module.address, initrd_physical))
	{
		return false;
	}

	ZeroBytes(modules, sizeof(BootModuleInfo) * kBootInfoMaxModules);
	modules[0].physical_start = initrd_physical;
	modules[0].length = initrd_module.size;
	modules[0].name = CopyBootString(arena, kBootInfoMaxModuleNameBytes, kInitrdModuleName);
	if(nullptr == modules[0].name)
	{
		return false;
	}

	boot_info.modules = modules;
	boot_info.module_count = 1;
	return true;
}

void PopulateFirmwarePointers(BootInfo &boot_info)
{
	uint64_t translated = 0;
	const auto *rsdp = g_rsdp_request.response;

	if(LiminePointerMapped(rsdp)
			&& (nullptr != rsdp->address)
			&& TranslateLimineVirtual((uint64_t)rsdp->address, translated))
	{
		boot_info.rsdp_physical = translated;
	}

	const auto *smbios = g_smbios_request.response;
	if(!LiminePointerMapped(smbios))
	{
		return;
	}

	if((nullptr != smbios->entry_64)
		&& TranslateLimineVirtual((uint64_t)smbios->entry_64, translated))
	{
		boot_info.smbios_physical = translated;
		return;
	}
	if((nullptr != smbios->entry_32)
		&& TranslateLimineVirtual((uint64_t)smbios->entry_32, translated))
	{
		boot_info.smbios_physical = translated;
	}
}

[[nodiscard]] bool PopulateFramebuffer(BootInfo &boot_info)
{
	const auto *response = g_framebuffer_request.response;
	if(!LiminePointerMapped(response) || (0 == response->framebuffer_count))
	{
		return true;
	}
	if(!LiminePointerMapped(response->framebuffers))
	{
		return true;
	}

	const auto *framebuffer = response->framebuffers[0];
	if(!LiminePointerMapped(framebuffer))
	{
		return true;
	}

	uint64_t framebuffer_physical = 0;
	if(!TranslateLimineVirtual((uint64_t)framebuffer->address, framebuffer_physical))
	{
		return true;
	}

	boot_info.framebuffer.physical_address = framebuffer_physical;
	boot_info.framebuffer.width = (uint32_t)framebuffer->width;
	boot_info.framebuffer.height = (uint32_t)framebuffer->height;
	boot_info.framebuffer.pitch_bytes = (uint32_t)framebuffer->pitch;
	boot_info.framebuffer.bits_per_pixel = framebuffer->bpp;
	boot_info.framebuffer.pixel_format = DetectFramebufferPixelFormat(framebuffer);
	return true;
}

[[nodiscard]] BootInfo *BuildBootInfo(const limine_file &initrd_module,
		uint64_t kernel_physical_start,
		uint64_t kernel_physical_end,
		uint64_t boot_info_storage_physical)
{
	WriteSerialLn("[limine-shim] BuildBootInfo start");
	LowHandoffBootInfoStorage *storage = MapBootInfoStorage(boot_info_storage_physical);
	BootInfo *boot_info = BootInfoStorage(storage);
	BootMemoryRegion *memory_map = BootMemoryMapStorage(storage);
	BootModuleInfo *modules = BootModuleStorage(storage);
	BootStringArena arena = CreateBootStringArena(storage);
	if((nullptr == boot_info) || (nullptr == memory_map) || (nullptr == modules) || (nullptr == arena.base))
	{
		WriteSerialLn("[limine-shim] handoff storage unavailable");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo storage ok");

	// The shared low-half kernel copies BootInfo immediately on entry, so a
	// small staging area carved out of the already-reserved kernel low-memory
	// window is sufficient and avoids depending on shim-owned higher-half .bss.
	ZeroBytes(boot_info, sizeof(BootInfo));
	ZeroBytes(memory_map, sizeof(BootMemoryRegion) * kBootInfoMaxMemoryRegions);
	ZeroBytes(modules, sizeof(BootModuleInfo) * kBootInfoMaxModules);
	ZeroBytes(arena.base, arena.capacity);
	WriteSerialLn("[limine-shim] bootinfo storage cleared");

	BootInfo &boot = *boot_info;
	boot.magic = kBootInfoMagic;
	boot.version = kBootInfoVersion;
	boot.source = BootSource::Limine;
	boot.kernel_physical_start = kernel_physical_start;
	boot.kernel_physical_end = kernel_physical_end;
	boot.text_console.columns = kBootTextColumns;
	boot.text_console.rows = kBootTextRows;
	boot.text_console.cursor_x = 0;
	boot.text_console.cursor_y = 0;

	if(!PopulateBootloaderInfo(boot, arena))
	{
		WriteSerialLn("[limine-shim] bootloader info handoff failed");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo bootloader ok");
	if(!PopulateCommandLine(boot, arena))
	{
		WriteSerialLn("[limine-shim] command line handoff failed");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo cmdline ok");
	if(!PopulateMemoryMap(boot, memory_map))
	{
		WriteSerialLn("[limine-shim] memory map handoff failed");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo memmap ok");
	if(!PopulateInitrdModule(boot, arena, initrd_module, modules))
	{
		WriteSerialLn("[limine-shim] initrd handoff failed");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo initrd ok");
	if(!PopulateFramebuffer(boot))
	{
		WriteSerialLn("[limine-shim] framebuffer handoff failed");
		return nullptr;
	}
	WriteSerialLn("[limine-shim] bootinfo framebuffer ok");
	PopulateFirmwarePointers(boot);
	WriteSerialLn("[limine-shim] bootinfo ready");
	return boot_info;
}
}

extern "C" [[noreturn]] void _start()
{
	InitSerial();
	asm volatile("cli");
	asm volatile("cld");
	WriteSerialLn("[limine-shim] start");
	if(!LIMINE_BASE_REVISION_SUPPORTED(g_limine_base_revision))
	{
		WriteSerialLn("[limine-shim] unsupported base revision");
		HaltForever();
	}

	const limine_file *kernel_file = FindModule(kKernelModuleName);
	const limine_file *initrd_file = FindModule(kInitrdModuleName);
	if((nullptr == kernel_file) || (nullptr == initrd_file))
	{
		WriteSerialLn("[limine-shim] missing required modules");
		HaltForever();
	}
	WriteSerialLn("[limine-shim] modules discovered");

	uint64_t entry_point = 0;
	uint64_t kernel_physical_start = 0;
	uint64_t kernel_physical_end = 0;
	uint64_t boot_info_storage_physical = 0;
	cpu *cpu_boot = nullptr;
	if(!LoadKernelImage(*kernel_file,
			entry_point,
			kernel_physical_start,
			kernel_physical_end,
			cpu_boot,
			boot_info_storage_physical))
	{
		WriteSerialLn("[limine-shim] low kernel load failed");
		HaltForever();
	}
	WriteSerial("[limine-shim] loaded low kernel entry=0x");
	WriteSerialHex(entry_point);
	WriteSerial(" cpu=0x");
	WriteSerialHex((uint64_t)cpu_boot);
	WriteSerialLn("");

	WriteSerialLn("[limine-shim] building BootInfo");
	BootInfo *boot_info = BuildBootInfo(*initrd_file,
			kernel_physical_start,
			kernel_physical_end,
			boot_info_storage_physical);
	if(nullptr == boot_info)
	{
		WriteSerialLn("[limine-shim] BootInfo build failed");
		HaltForever();
	}
	WriteSerialLn("[limine-shim] entering KernelMain");

	limine_enter_kernel((void (*)(BootInfo *, cpu *))entry_point, boot_info, cpu_boot);
}
