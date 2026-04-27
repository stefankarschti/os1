// ACPI table parser for platform discovery. It normalizes RSDP/XSDT/RSDT,
// MADT, and MCFG content into platform-state records consumed by topology,
// interrupt routing, and PCI enumeration.
#include "platform/acpi.h"

#include "arch/x86_64/cpu/cpu.h"
#include "debug/debug.h"
#include "handoff/memory_layout.h"
#include "util/string.h"
#include "mm/virtual_memory.h"
#include "arch/x86_64/cpu/x86.h"

namespace
{
constexpr uint64_t kAcpiMaxTableLength = 1ull << 20;
constexpr uint8_t kAcpiIsaBus = 0;
constexpr uint8_t kAcpiMadtTypeLocalApic = 0;
constexpr uint8_t kAcpiMadtTypeIoApic = 1;
constexpr uint8_t kAcpiMadtTypeInterruptOverride = 2;
constexpr uint8_t kAcpiMadtTypeLocalApicAddressOverride = 5;

struct [[gnu::packed]] AcpiRsdp
{
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum;
	uint8_t reserved[3];
};

struct [[gnu::packed]] AcpiSdtHeader
{
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
};

struct [[gnu::packed]] AcpiMadt
{
	AcpiSdtHeader header;
	uint32_t lapic_address;
	uint32_t flags;
};

struct [[gnu::packed]] AcpiMadtLocalApic
{
	uint8_t type;
	uint8_t length;
	uint8_t acpi_processor_id;
	uint8_t apic_id;
	uint32_t flags;
};

struct [[gnu::packed]] AcpiMadtIoApic
{
	uint8_t type;
	uint8_t length;
	uint8_t ioapic_id;
	uint8_t reserved;
	uint32_t ioapic_address;
	uint32_t gsi_base;
};

struct [[gnu::packed]] AcpiMadtInterruptOverride
{
	uint8_t type;
	uint8_t length;
	uint8_t bus;
	uint8_t source_irq;
	uint32_t global_irq;
	uint16_t flags;
};

struct [[gnu::packed]] AcpiMadtLocalApicAddressOverride
{
	uint8_t type;
	uint8_t length;
	uint16_t reserved;
	uint64_t lapic_address;
};

struct [[gnu::packed]] AcpiMcfg
{
	AcpiSdtHeader header;
	uint64_t reserved;
};

struct [[gnu::packed]] AcpiMcfgEntry
{
	uint64_t base_address;
	uint16_t segment_group;
	uint8_t bus_start;
	uint8_t bus_end;
	uint32_t reserved;
};

struct AcpiOutput
{
	uint64_t &lapic_base;
	CpuInfo *cpus;
	size_t &cpu_count;
	IoApicInfo *ioapics;
	size_t &ioapic_count;
	InterruptOverride *overrides;
	size_t &override_count;
	PciEcamRegion *ecam_regions;
	size_t &ecam_region_count;
};

[[nodiscard]] inline uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
	return value & ~(alignment - 1);
}


[[nodiscard]] inline uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

bool MapIdentityRange(VirtualMemory &vm, uint64_t physical_start, uint64_t length)
{
	if((0 == length) || (0 == physical_start))
	{
		return true;
	}

	const uint64_t start = AlignDown(physical_start, kPageSize);
	const uint64_t end = AlignUp(physical_start + length, kPageSize);
	return vm.MapPhysical(start,
			start,
			(end - start) / kPageSize,
			PageFlags::Present | PageFlags::Write);
}

[[nodiscard]] bool ValidateChecksum(const void *base, size_t length)
{
	const auto *bytes = static_cast<const uint8_t *>(base);
	uint8_t sum = 0;
	for(size_t i = 0; i < length; ++i)
	{
		sum = static_cast<uint8_t>(sum + bytes[i]);
	}
	return 0 == sum;
}

[[nodiscard]] bool SignatureEquals(const char *left, const char *right, size_t length)
{
	return 0 == memcmp(left, right, length);
}

[[nodiscard]] uint8_t CurrentApicId()
{
	cpuinfo info{};
	cpuid(1, &info);
	return static_cast<uint8_t>((info.ebx >> 24) & 0xFFu);
}

[[nodiscard]] bool MapAcpiRange(VirtualMemory &kernel_vm, uint64_t physical_address, uint64_t length)
{
	if((0 == physical_address) || (0 == length))
	{
		return false;
	}
	return MapIdentityRange(kernel_vm, physical_address, length);
}

template<typename T>
[[nodiscard]] const T *MapAcpiObject(VirtualMemory &kernel_vm, uint64_t physical_address)
{
	if(!MapAcpiRange(kernel_vm, physical_address, sizeof(T)))
	{
		return nullptr;
	}
	return reinterpret_cast<const T *>(physical_address);
}

[[nodiscard]] const AcpiSdtHeader *MapAcpiTable(VirtualMemory &kernel_vm,
		uint64_t physical_address,
		const char *expected_signature)
{
	const AcpiSdtHeader *header = MapAcpiObject<AcpiSdtHeader>(kernel_vm, physical_address);
	if(nullptr == header)
	{
		return nullptr;
	}
	if(expected_signature && !SignatureEquals(header->signature, expected_signature, 4))
	{
		debug("acpi: unexpected signature at 0x")(physical_address, 16)();
		return nullptr;
	}
	if((header->length < sizeof(AcpiSdtHeader)) || (header->length > kAcpiMaxTableLength))
	{
		debug("acpi: invalid table length 0x")(header->length, 16)(" at 0x")(physical_address, 16)();
		return nullptr;
	}
	if(!MapAcpiRange(kernel_vm, physical_address, header->length))
	{
		return nullptr;
	}
	header = reinterpret_cast<const AcpiSdtHeader *>(physical_address);
	if(!ValidateChecksum(header, header->length))
	{
		debug("acpi: checksum failed at 0x")(physical_address, 16)();
		return nullptr;
	}
	return header;
}

[[nodiscard]] bool AddInterruptOverride(AcpiOutput &output, uint8_t bus_irq, uint32_t global_irq, uint16_t flags)
{
	if(output.override_count >= kPlatformMaxInterruptOverrides)
	{
		debug("acpi: interrupt override table full")();
		return false;
	}
	InterruptOverride &entry = output.overrides[output.override_count++];
	entry.bus_irq = bus_irq;
	entry.flags = flags;
	entry.global_irq = global_irq;
	return true;
}

[[nodiscard]] bool ParseMadt(VirtualMemory &kernel_vm, uint64_t physical_address, AcpiOutput &output)
{
	const auto *header = MapAcpiTable(kernel_vm, physical_address, "APIC");
	if(nullptr == header)
	{
		return false;
	}

	const auto *madt = reinterpret_cast<const AcpiMadt *>(header);
	output.lapic_base = madt->lapic_address;
	output.cpu_count = 0;
	output.ioapic_count = 0;
	output.override_count = 0;

	const auto *cursor = reinterpret_cast<const uint8_t *>(madt + 1);
	const auto *end = reinterpret_cast<const uint8_t *>(madt) + madt->header.length;
	while(cursor < end)
	{
		if((cursor + 2) > end)
		{
			debug("acpi: MADT truncated")();
			return false;
		}
		const uint8_t type = cursor[0];
		const uint8_t length = cursor[1];
		if((length < 2) || ((cursor + length) > end))
		{
			debug("acpi: MADT entry length invalid")();
			return false;
		}

		switch(type)
		{
		case kAcpiMadtTypeLocalApic:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtLocalApic *>(cursor);
				if(entry->flags & 0x1u)
				{
					if(output.cpu_count >= kPlatformMaxCpus)
					{
						debug("acpi: CPU table full")();
						return false;
					}
					CpuInfo &cpu = output.cpus[output.cpu_count++];
					cpu.apic_id = entry->apic_id;
					cpu.enabled = true;
					cpu.is_bsp = false;
				}
			}
			break;
		case kAcpiMadtTypeIoApic:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtIoApic *>(cursor);
				if(output.ioapic_count >= kPlatformMaxIoApics)
				{
					debug("acpi: IOAPIC table full")();
					return false;
				}
				IoApicInfo &ioapic_info = output.ioapics[output.ioapic_count++];
				ioapic_info.id = entry->ioapic_id;
				ioapic_info.address = entry->ioapic_address;
				ioapic_info.gsi_base = entry->gsi_base;
			}
			break;
		case kAcpiMadtTypeInterruptOverride:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtInterruptOverride *>(cursor);
				if(entry->bus == kAcpiIsaBus)
				{
					if(!AddInterruptOverride(output, entry->source_irq, entry->global_irq, entry->flags))
					{
						return false;
					}
				}
			}
			break;
		case kAcpiMadtTypeLocalApicAddressOverride:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtLocalApicAddressOverride *>(cursor);
				output.lapic_base = entry->lapic_address;
			}
			break;
		default:
			break;
		}

		cursor += length;
	}

	if((0 == output.cpu_count) || (0 == output.ioapic_count) || (0 == output.lapic_base))
	{
		debug("acpi: MADT missing required topology")();
		return false;
	}

	const uint8_t bsp_apic_id = CurrentApicId();
	bool found_bsp = false;
	for(size_t i = 0; i < output.cpu_count; ++i)
	{
		if(output.cpus[i].apic_id == bsp_apic_id)
		{
			output.cpus[i].is_bsp = true;
			found_bsp = true;
			break;
		}
	}
	if(!found_bsp)
	{
		debug("acpi: BSP APIC ID not found in MADT")();
		return false;
	}

	debug("acpi: MADT ready cpus=")(output.cpu_count)
		(" ioapics=")(output.ioapic_count)
		(" overrides=")(output.override_count)();
	return true;
}

[[nodiscard]] bool ParseMcfg(VirtualMemory &kernel_vm, uint64_t physical_address, AcpiOutput &output)
{
	const auto *header = MapAcpiTable(kernel_vm, physical_address, "MCFG");
	if(nullptr == header)
	{
		return false;
	}
	if(header->length < sizeof(AcpiMcfg))
	{
		debug("acpi: MCFG too short")();
		return false;
	}

	output.ecam_region_count = 0;
	const uint32_t payload_length = header->length - sizeof(AcpiMcfg);
	if(0 != (payload_length % sizeof(AcpiMcfgEntry)))
	{
		debug("acpi: MCFG length misaligned")();
		return false;
	}

	const auto *entries = reinterpret_cast<const AcpiMcfgEntry *>(
			reinterpret_cast<const uint8_t *>(header) + sizeof(AcpiMcfg));
	const size_t entry_count = payload_length / sizeof(AcpiMcfgEntry);
	if(0 == entry_count)
	{
		debug("acpi: MCFG contains no ECAM regions")();
		return false;
	}

	for(size_t i = 0; i < entry_count; ++i)
	{
		if(output.ecam_region_count >= kPlatformMaxPciEcamRegions)
		{
			debug("acpi: ECAM region table full")();
			return false;
		}
		if(entries[i].bus_start > entries[i].bus_end)
		{
			debug("acpi: invalid ECAM bus range")();
			return false;
		}

		PciEcamRegion &region = output.ecam_regions[output.ecam_region_count++];
		region.base_address = entries[i].base_address;
		region.segment_group = entries[i].segment_group;
		region.bus_start = entries[i].bus_start;
		region.bus_end = entries[i].bus_end;
	}

	debug("acpi: MCFG ready regions=")(output.ecam_region_count)();
	return true;
}

[[nodiscard]] bool ResolveAcpiTables(VirtualMemory &kernel_vm,
		const BootInfo &boot_info,
		uint64_t &madt_physical,
		uint64_t &mcfg_physical)
{
	madt_physical = 0;
	mcfg_physical = 0;
	if(0 == boot_info.rsdp_physical)
	{
		debug("acpi: boot did not supply an RSDP")();
		return false;
	}
	if(!MapAcpiRange(kernel_vm, boot_info.rsdp_physical, sizeof(AcpiRsdp)))
	{
		return false;
	}

	const auto *rsdp = reinterpret_cast<const AcpiRsdp *>(boot_info.rsdp_physical);
	if(!SignatureEquals(rsdp->signature, "RSD PTR ", 8))
	{
		debug("acpi: RSDP signature invalid")();
		return false;
	}
	if(!ValidateChecksum(rsdp, 20))
	{
		debug("acpi: RSDP checksum invalid")();
		return false;
	}
	debug("boot rsdp physical=0x")(boot_info.rsdp_physical, 16)();

	uint64_t root_table_physical = 0;
	bool use_xsdt = false;
	if((rsdp->revision >= 2) && (rsdp->length >= sizeof(AcpiRsdp)) && (0 != rsdp->xsdt_address))
	{
		if(!ValidateChecksum(rsdp, rsdp->length))
		{
			debug("acpi: XSDP extended checksum invalid")();
			return false;
		}
		root_table_physical = rsdp->xsdt_address;
		use_xsdt = true;
	}
	else if(0 != rsdp->rsdt_address)
	{
		root_table_physical = rsdp->rsdt_address;
	}
	else
	{
		debug("acpi: neither XSDT nor RSDT is available")();
		return false;
	}

	const AcpiSdtHeader *root = MapAcpiTable(kernel_vm,
			root_table_physical,
			use_xsdt ? "XSDT" : "RSDT");
	if((nullptr == root) && use_xsdt && (0 != rsdp->rsdt_address))
	{
		debug("acpi: XSDT unavailable, falling back to RSDT")();
		root = MapAcpiTable(kernel_vm, rsdp->rsdt_address, "RSDT");
		use_xsdt = false;
	}
	if(nullptr == root)
	{
		return false;
	}

	const size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
	if(root->length < sizeof(AcpiSdtHeader))
	{
		return false;
	}
	const uint32_t payload_length = root->length - sizeof(AcpiSdtHeader);
	if(0 != (payload_length % entry_size))
	{
		debug("acpi: root table length misaligned")();
		return false;
	}

	const size_t entry_count = payload_length / entry_size;
	const auto *cursor = reinterpret_cast<const uint8_t *>(root) + sizeof(AcpiSdtHeader);
	for(size_t i = 0; i < entry_count; ++i)
	{
		const uint64_t entry_physical = use_xsdt
				? reinterpret_cast<const uint64_t *>(cursor)[i]
				: reinterpret_cast<const uint32_t *>(cursor)[i];
		const auto *entry_header = MapAcpiObject<AcpiSdtHeader>(kernel_vm, entry_physical);
		if(nullptr == entry_header)
		{
			return false;
		}
		if(SignatureEquals(entry_header->signature, "APIC", 4))
		{
			madt_physical = entry_physical;
		}
		else if(SignatureEquals(entry_header->signature, "MCFG", 4))
		{
			mcfg_physical = entry_physical;
		}
	}

	return (0 != madt_physical) && (0 != mcfg_physical);
}
}

bool DiscoverAcpiPlatform(VirtualMemory &kernel_vm,
		const BootInfo &boot_info,
		uint64_t &lapic_base,
		CpuInfo *cpus,
		size_t &cpu_count,
		IoApicInfo *ioapics,
		size_t &ioapic_count,
		InterruptOverride *overrides,
		size_t &override_count,
		PciEcamRegion *ecam_regions,
		size_t &ecam_region_count)
{
	if((nullptr == cpus) || (nullptr == ioapics) || (nullptr == overrides) || (nullptr == ecam_regions))
	{
		return false;
	}

	uint64_t madt_physical = 0;
	uint64_t mcfg_physical = 0;
	if(!ResolveAcpiTables(kernel_vm, boot_info, madt_physical, mcfg_physical))
	{
		return false;
	}

	AcpiOutput output{
		.lapic_base = lapic_base,
		.cpus = cpus,
		.cpu_count = cpu_count,
		.ioapics = ioapics,
		.ioapic_count = ioapic_count,
		.overrides = overrides,
		.override_count = override_count,
		.ecam_regions = ecam_regions,
		.ecam_region_count = ecam_region_count,
	};
	return ParseMadt(kernel_vm, madt_physical, output)
		&& ParseMcfg(kernel_vm, mcfg_physical, output);
}