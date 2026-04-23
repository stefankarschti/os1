#include "platform.h"

#include "bootinfo.h"
#include "cpu.h"
#include "debug.h"
#include "interrupt.h"
#include "ioapic.h"
#include "lapic.h"
#include "memory_layout.h"
#include "mp.h"
#include "pageframe.h"
#include "string.h"
#include "virtualmemory.h"
#include "x86.h"

extern PageFrameContainer page_frames;

namespace
{
constexpr uint64_t kAcpiMaxTableLength = 1ull << 20;
constexpr uint8_t kAcpiIsaBus = 0;
constexpr uint8_t kAcpiMadtTypeLocalApic = 0;
constexpr uint8_t kAcpiMadtTypeIoApic = 1;
constexpr uint8_t kAcpiMadtTypeInterruptOverride = 2;
constexpr uint8_t kAcpiMadtTypeLocalApicAddressOverride = 5;
constexpr uint8_t kPciCapabilityVendorSpecific = 0x09;
constexpr uint16_t kPciVendorVirtio = 0x1AF4;
constexpr uint16_t kPciDeviceVirtioBlkModern = 0x1042;
constexpr uint16_t kVirtioNoVector = 0xFFFF;
constexpr uint32_t kVirtioStatusAcknowledge = 1u << 0;
constexpr uint32_t kVirtioStatusDriver = 1u << 1;
constexpr uint32_t kVirtioStatusDriverOk = 1u << 2;
constexpr uint32_t kVirtioStatusFeaturesOk = 1u << 3;
constexpr uint64_t kVirtioFeatureVersion1 = 1ull << 32;
constexpr uint8_t kVirtioPciCapCommonCfg = 1;
constexpr uint8_t kVirtioPciCapNotifyCfg = 2;
constexpr uint8_t kVirtioPciCapDeviceCfg = 4;
constexpr uint32_t kVirtioBlkRequestIn = 0;
constexpr uint16_t kVirtqDescFlagNext = 1u << 0;
constexpr uint16_t kVirtqDescFlagWrite = 1u << 1;
constexpr uint16_t kVirtioBlkQueueTargetSize = 8;
constexpr uint64_t kVirtioSectorSize = 512;
constexpr const char *kVirtioSector0Prefix = "OS1 VIRTIO TEST DISK SECTOR 0 SIGNATURE";
constexpr const char *kVirtioSector1Prefix = "OS1 VIRTIO TEST DISK SECTOR 1 PAYLOAD";

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

struct [[gnu::packed]] VirtioPciCapability
{
	uint8_t cap_vndr;
	uint8_t cap_next;
	uint8_t cap_len;
	uint8_t cfg_type;
	uint8_t bar;
	uint8_t id;
	uint8_t padding[2];
	uint32_t offset;
	uint32_t length;
};

struct [[gnu::packed]] VirtioPciNotifyCapability
{
	VirtioPciCapability common;
	uint32_t notify_off_multiplier;
};

struct [[gnu::packed]] VirtioPciCommonCfg
{
	uint32_t device_feature_select;
	uint32_t device_feature;
	uint32_t driver_feature_select;
	uint32_t driver_feature;
	uint16_t msix_config;
	uint16_t num_queues;
	uint8_t device_status;
	uint8_t config_generation;
	uint16_t queue_select;
	uint16_t queue_size;
	uint16_t queue_msix_vector;
	uint16_t queue_enable;
	uint16_t queue_notify_off;
	uint64_t queue_desc;
	uint64_t queue_driver;
	uint64_t queue_device;
};

struct [[gnu::packed]] VirtioBlkConfig
{
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectors;
	uint32_t blk_size;
};

struct [[gnu::packed]] VirtioBlkRequestHeader
{
	uint32_t type;
	uint32_t reserved;
	uint64_t sector;
};

struct [[gnu::packed]] VirtqDesc
{
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
};

struct [[gnu::packed]] VirtqAvail
{
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[kVirtioBlkQueueTargetSize];
	uint16_t used_event;
};

struct [[gnu::packed]] VirtqUsedElem
{
	uint32_t id;
	uint32_t len;
};

struct [[gnu::packed]] VirtqUsed
{
	uint16_t flags;
	uint16_t idx;
	VirtqUsedElem ring[kVirtioBlkQueueTargetSize];
	uint16_t avail_event;
};

struct VirtioBlkState
{
	bool present;
	uint16_t queue_size;
	uint16_t pci_index;
	uint64_t capacity_sectors;
	volatile VirtioPciCommonCfg *common_cfg;
	volatile VirtioBlkConfig *device_cfg;
	volatile uint16_t *notify_register;
	uint32_t notify_multiplier;
	uint64_t queue_memory;
	VirtqDesc *desc;
	VirtqAvail *avail;
	volatile VirtqUsed *used;
	uint16_t last_used_idx;
	uint64_t request_memory;
	VirtioBlkRequestHeader *request_header;
	uint8_t *request_data;
	uint8_t *request_status;
};

struct PlatformState
{
	bool initialized;
	bool acpi_active;
	bool used_legacy_mp_fallback;
	uint64_t lapic_base;
	size_t cpu_count;
	CpuInfo cpus[kPlatformMaxCpus];
	size_t ioapic_count;
	IoApicInfo ioapics[kPlatformMaxIoApics];
	size_t override_count;
	InterruptOverride overrides[kPlatformMaxInterruptOverrides];
	size_t ecam_region_count;
	PciEcamRegion ecam_regions[kPlatformMaxPciEcamRegions];
	size_t device_count;
	PciDevice devices[kPlatformMaxPciDevices];
	VirtioBlkDevice virtio_blk_public;
	VirtioBlkState virtio_blk_state;
};

constinit PlatformState g_platform{};

[[nodiscard]] inline uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
	return value & ~(alignment - 1);
}

[[nodiscard]] inline uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] uint16_t MinU16(uint16_t left, uint16_t right)
{
	return (left < right) ? left : right;
}

void WriteDeviceStatus(volatile VirtioPciCommonCfg *common_cfg, uint8_t status)
{
	if(nullptr != common_cfg)
	{
		common_cfg->device_status = status;
	}
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

[[nodiscard]] bool AddInterruptOverride(uint8_t bus_irq, uint32_t global_irq, uint16_t flags)
{
	if(g_platform.override_count >= kPlatformMaxInterruptOverrides)
	{
		debug("acpi: interrupt override table full")();
		return false;
	}
	InterruptOverride &entry = g_platform.overrides[g_platform.override_count++];
	entry.bus_irq = bus_irq;
	entry.flags = flags;
	entry.global_irq = global_irq;
	return true;
}

[[nodiscard]] bool ParseMadt(VirtualMemory &kernel_vm, uint64_t physical_address)
{
	const auto *header = MapAcpiTable(kernel_vm, physical_address, "APIC");
	if(nullptr == header)
	{
		return false;
	}

	const auto *madt = reinterpret_cast<const AcpiMadt *>(header);
	g_platform.lapic_base = madt->lapic_address;
	g_platform.cpu_count = 0;
	g_platform.ioapic_count = 0;
	g_platform.override_count = 0;

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
					if(g_platform.cpu_count >= kPlatformMaxCpus)
					{
						debug("acpi: CPU table full")();
						return false;
					}
					CpuInfo &cpu = g_platform.cpus[g_platform.cpu_count++];
					cpu.apic_id = entry->apic_id;
					cpu.enabled = true;
					cpu.is_bsp = false;
				}
			}
			break;
		case kAcpiMadtTypeIoApic:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtIoApic *>(cursor);
				if(g_platform.ioapic_count >= kPlatformMaxIoApics)
				{
					debug("acpi: IOAPIC table full")();
					return false;
				}
				IoApicInfo &ioapic_info = g_platform.ioapics[g_platform.ioapic_count++];
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
					if(!AddInterruptOverride(entry->source_irq, entry->global_irq, entry->flags))
					{
						return false;
					}
				}
			}
			break;
		case kAcpiMadtTypeLocalApicAddressOverride:
			{
				const auto *entry = reinterpret_cast<const AcpiMadtLocalApicAddressOverride *>(cursor);
				g_platform.lapic_base = entry->lapic_address;
			}
			break;
		default:
			break;
		}

		cursor += length;
	}

	if((0 == g_platform.cpu_count) || (0 == g_platform.ioapic_count) || (0 == g_platform.lapic_base))
	{
		debug("acpi: MADT missing required topology")();
		return false;
	}

	const uint8_t bsp_apic_id = CurrentApicId();
	bool found_bsp = false;
	for(size_t i = 0; i < g_platform.cpu_count; ++i)
	{
		if(g_platform.cpus[i].apic_id == bsp_apic_id)
		{
			g_platform.cpus[i].is_bsp = true;
			found_bsp = true;
			break;
		}
	}
	if(!found_bsp)
	{
		debug("acpi: BSP APIC ID not found in MADT")();
		return false;
	}

	debug("acpi: MADT ready cpus=")(g_platform.cpu_count)
		(" ioapics=")(g_platform.ioapic_count)
		(" overrides=")(g_platform.override_count)();
	return true;
}

[[nodiscard]] bool ParseMcfg(VirtualMemory &kernel_vm, uint64_t physical_address)
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

	g_platform.ecam_region_count = 0;
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
		if(g_platform.ecam_region_count >= kPlatformMaxPciEcamRegions)
		{
			debug("acpi: ECAM region table full")();
			return false;
		}
		if(entries[i].bus_start > entries[i].bus_end)
		{
			debug("acpi: invalid ECAM bus range")();
			return false;
		}

		PciEcamRegion &region = g_platform.ecam_regions[g_platform.ecam_region_count++];
		region.base_address = entries[i].base_address;
		region.segment_group = entries[i].segment_group;
		region.bus_start = entries[i].bus_start;
		region.bus_end = entries[i].bus_end;
	}

	debug("acpi: MCFG ready regions=")(g_platform.ecam_region_count)();
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

[[nodiscard]] bool AllocateCpusFromTopology()
{
	ismp = 1;
	ncpu = 0;
	ioapicid = 0;
	ioapic = nullptr;

	for(size_t i = 0; i < g_platform.cpu_count; ++i)
	{
		const CpuInfo &cpu_info = g_platform.cpus[i];
		if(!cpu_info.enabled)
		{
			continue;
		}
		if(cpu_info.apic_id > 0xFFu)
		{
			debug("acpi: APIC ID exceeds current cpu::id width")();
			return false;
		}
		debug("acpi: CPU apic_id=")(cpu_info.apic_id)(" bsp=")(cpu_info.is_bsp ? 1 : 0)();

		cpu *entry = cpu_info.is_bsp ? g_cpu_boot : cpu_alloc();
		if(nullptr == entry)
		{
			return false;
		}
		entry->id = static_cast<uint8_t>(cpu_info.apic_id);
		++ncpu;
	}

	const IoApicInfo &primary = g_platform.ioapics[0];
	debug("acpi: primary ioapic id=")(primary.id)(" addr=0x")(primary.address, 16)(" gsi_base=")(primary.gsi_base)();
	ioapicid = static_cast<uint8_t>(primary.id);
	ioapic = reinterpret_cast<volatile struct ioapic *>(static_cast<uint64_t>(primary.address));
	lapic = reinterpret_cast<volatile uint32_t *>(g_platform.lapic_base);
	ioapic_set_primary(primary.gsi_base);
	return true;
}

[[nodiscard]] uint8_t PciRead8(uint64_t config_physical, uint16_t offset)
{
	return *reinterpret_cast<volatile uint8_t *>(config_physical + offset);
}

[[nodiscard]] uint16_t PciRead16(uint64_t config_physical, uint16_t offset)
{
	return *reinterpret_cast<volatile uint16_t *>(config_physical + offset);
}

[[nodiscard]] uint32_t PciRead32(uint64_t config_physical, uint16_t offset)
{
	return *reinterpret_cast<volatile uint32_t *>(config_physical + offset);
}

void PciWrite16(uint64_t config_physical, uint16_t offset, uint16_t value)
{
	*reinterpret_cast<volatile uint16_t *>(config_physical + offset) = value;
}

void PciWrite32(uint64_t config_physical, uint16_t offset, uint32_t value)
{
	*reinterpret_cast<volatile uint32_t *>(config_physical + offset) = value;
}

[[nodiscard]] uint8_t HeaderTypeKind(uint8_t header_type)
{
	return header_type & 0x7Fu;
}

void SizePciBars(PciDevice &device)
{
	const uint8_t header_type = HeaderTypeKind(device.header_type);
	const uint8_t bar_limit = (0x01u == header_type) ? 2u : ((0x00u == header_type) ? 6u : 0u);
	device.bar_count = bar_limit;
	if(0 == bar_limit)
	{
		return;
	}

	const uint16_t command = PciRead16(device.config_physical, 0x04);
	PciWrite16(device.config_physical, 0x04, static_cast<uint16_t>(command & ~0x3u));

	for(uint8_t index = 0; index < bar_limit; ++index)
	{
		const uint16_t offset = static_cast<uint16_t>(0x10 + index * 4);
		const uint32_t original = PciRead32(device.config_physical, offset);
		if(0 == original)
		{
			continue;
		}

		PciBarInfo &bar = device.bars[index];
		if(original & 0x1u)
		{
			PciWrite32(device.config_physical, offset, 0xFFFFFFFFu);
			const uint32_t sized = PciRead32(device.config_physical, offset);
			PciWrite32(device.config_physical, offset, original);
			const uint32_t mask = sized & ~0x3u;
			if(0 == mask)
			{
				continue;
			}
			bar.base = original & ~0x3u;
			bar.size = (~static_cast<uint64_t>(mask) + 1u) & 0xFFFFFFFFull;
			bar.type = PciBarType::Io;
			continue;
		}

		const bool is_64_bit = 0x2u == ((original >> 1) & 0x3u);
		const uint32_t original_high = is_64_bit ? PciRead32(device.config_physical, offset + 4) : 0u;
		PciWrite32(device.config_physical, offset, 0xFFFFFFFFu);
		if(is_64_bit)
		{
			PciWrite32(device.config_physical, offset + 4, 0xFFFFFFFFu);
		}
		const uint32_t sized_low = PciRead32(device.config_physical, offset);
		const uint32_t sized_high = is_64_bit ? PciRead32(device.config_physical, offset + 4) : 0u;
		PciWrite32(device.config_physical, offset, original);
		if(is_64_bit)
		{
			PciWrite32(device.config_physical, offset + 4, original_high);
		}

		uint64_t base = original & ~0xFull;
		uint64_t size_mask = sized_low & ~0xFull;
		PciBarType type = PciBarType::Mmio32;
		if(is_64_bit)
		{
			base |= static_cast<uint64_t>(original_high) << 32;
			size_mask |= static_cast<uint64_t>(sized_high) << 32;
			type = PciBarType::Mmio64;
		}
		if(0 == size_mask)
		{
			if(is_64_bit)
			{
				++index;
			}
			continue;
		}

		bar.base = base;
		bar.size = ~size_mask + 1u;
		bar.type = type;
		if(is_64_bit && (index < 5u))
		{
			device.bars[index + 1].type = PciBarType::Unused;
			++index;
		}
	}

	PciWrite16(device.config_physical, 0x04, command);
}

[[nodiscard]] bool RecordPciDevice(const PciEcamRegion &region,
		uint8_t bus,
		uint8_t slot,
		uint8_t function,
		uint64_t config_physical)
{
	if(g_platform.device_count >= kPlatformMaxPciDevices)
	{
		debug("pci: device table full")();
		return false;
	}

	PciDevice &device = g_platform.devices[g_platform.device_count++];
	memset(&device, 0, sizeof(device));
	device.segment_group = region.segment_group;
	device.bus = bus;
	device.slot = slot;
	device.function = function;
	device.config_physical = config_physical;
	device.vendor_id = PciRead16(config_physical, 0x00);
	device.device_id = PciRead16(config_physical, 0x02);
	device.revision = PciRead8(config_physical, 0x08);
	device.prog_if = PciRead8(config_physical, 0x09);
	device.subclass = PciRead8(config_physical, 0x0A);
	device.class_code = PciRead8(config_physical, 0x0B);
	device.header_type = PciRead8(config_physical, 0x0E);
	device.interrupt_line = PciRead8(config_physical, 0x3C);
	device.interrupt_pin = PciRead8(config_physical, 0x3D);

	const uint16_t status = PciRead16(config_physical, 0x06);
	if(status & (1u << 4))
	{
		device.capability_pointer = PciRead8(config_physical, 0x34);
	}

	SizePciBars(device);
	return true;
}

[[nodiscard]] bool EnumeratePci(VirtualMemory &kernel_vm)
{
	g_platform.device_count = 0;
	for(size_t region_index = 0; region_index < g_platform.ecam_region_count; ++region_index)
	{
		const PciEcamRegion &region = g_platform.ecam_regions[region_index];
		const uint64_t region_size = static_cast<uint64_t>(region.bus_end - region.bus_start + 1u) << 20;
		if(!MapIdentityRange(kernel_vm, region.base_address, region_size))
		{
			return false;
		}

		for(uint16_t bus = region.bus_start; bus <= region.bus_end; ++bus)
		{
			for(uint8_t slot = 0; slot < 32; ++slot)
			{
				const uint64_t function0 = region.base_address
						+ (static_cast<uint64_t>(bus - region.bus_start) << 20)
						+ (static_cast<uint64_t>(slot) << 15);
				const uint16_t vendor0 = PciRead16(function0, 0x00);
				if(0xFFFFu == vendor0)
				{
					continue;
				}

				const uint8_t header_type = PciRead8(function0, 0x0E);
				const uint8_t function_limit = (header_type & 0x80u) ? 8u : 1u;
				for(uint8_t function = 0; function < function_limit; ++function)
				{
					const uint64_t config_physical = function0 + (static_cast<uint64_t>(function) << 12);
					if(0xFFFFu == PciRead16(config_physical, 0x00))
					{
						continue;
					}
					if(!RecordPciDevice(region, static_cast<uint8_t>(bus), slot, function, config_physical))
					{
						return false;
					}
				}
			}
		}
	}

	debug("pci: enumerated devices=")(g_platform.device_count)();
	return true;
}

[[nodiscard]] bool MapBarForCapability(VirtualMemory &kernel_vm, const PciDevice &device, uint8_t bar_index)
{
	if(bar_index >= 6u)
	{
		return false;
	}
	const PciBarInfo &bar = device.bars[bar_index];
	if((0 == bar.base) || (0 == bar.size))
	{
		return false;
	}
	if((PciBarType::Mmio32 != bar.type) && (PciBarType::Mmio64 != bar.type))
	{
		return false;
	}
	return MapIdentityRange(kernel_vm, bar.base, bar.size);
}

[[nodiscard]] bool ProbeVirtioBlkDevice(VirtualMemory &kernel_vm, size_t device_index)
{
	PciDevice &device = g_platform.devices[device_index];
	if((device.vendor_id != kPciVendorVirtio) || (device.device_id != kPciDeviceVirtioBlkModern))
	{
		return true;
	}
	if(g_platform.virtio_blk_state.present)
	{
		debug("virtio-blk: additional device ignored")();
		return true;
	}

	uint8_t common_bar = 0xFFu;
	uint32_t common_offset = 0;
	uint32_t common_length = 0;
	uint8_t notify_bar = 0xFFu;
	uint32_t notify_offset = 0;
	uint32_t notify_length = 0;
	uint32_t notify_multiplier = 0;
	uint8_t device_bar = 0xFFu;
	uint32_t device_offset = 0;
	uint32_t device_length = 0;

	uint8_t capability = device.capability_pointer;
	for(size_t guard = 0; (0 != capability) && (guard < 48); ++guard)
	{
		if(capability < 0x40u)
		{
			break;
		}
		const uint8_t cap_id = PciRead8(device.config_physical, capability);
		const uint8_t cap_next = PciRead8(device.config_physical, capability + 1);
		if(kPciCapabilityVendorSpecific == cap_id)
		{
			const VirtioPciCapability cap{
				.cap_vndr = cap_id,
				.cap_next = cap_next,
				.cap_len = PciRead8(device.config_physical, capability + 2),
				.cfg_type = PciRead8(device.config_physical, capability + 3),
				.bar = PciRead8(device.config_physical, capability + 4),
				.id = PciRead8(device.config_physical, capability + 5),
				.padding = {PciRead8(device.config_physical, capability + 6), PciRead8(device.config_physical, capability + 7)},
				.offset = PciRead32(device.config_physical, capability + 8),
				.length = PciRead32(device.config_physical, capability + 12),
			};

			switch(cap.cfg_type)
			{
			case kVirtioPciCapCommonCfg:
				common_bar = cap.bar;
				common_offset = cap.offset;
				common_length = cap.length;
				break;
			case kVirtioPciCapNotifyCfg:
				notify_bar = cap.bar;
				notify_offset = cap.offset;
				notify_length = cap.length;
				notify_multiplier = PciRead32(device.config_physical, capability + 16);
				break;
			case kVirtioPciCapDeviceCfg:
				device_bar = cap.bar;
				device_offset = cap.offset;
				device_length = cap.length;
				break;
			default:
				break;
			}
		}
		if(0 == cap_next)
		{
			break;
		}
		capability = cap_next;
	}

	if((0xFFu == common_bar) || (0xFFu == notify_bar) || (0xFFu == device_bar))
	{
		debug("virtio-blk: required modern PCI capabilities missing")();
		return false;
	}
	if((common_length < sizeof(VirtioPciCommonCfg)) || (device_length < sizeof(VirtioBlkConfig)) || (0 == notify_multiplier))
	{
		debug("virtio-blk: invalid capability lengths")();
		return false;
	}
	if(!MapBarForCapability(kernel_vm, device, common_bar)
		|| !MapBarForCapability(kernel_vm, device, notify_bar)
		|| !MapBarForCapability(kernel_vm, device, device_bar))
	{
		debug("virtio-blk: BAR mapping failed")();
		return false;
	}

	const PciBarInfo &common_bar_info = device.bars[common_bar];
	const PciBarInfo &notify_bar_info = device.bars[notify_bar];
	const PciBarInfo &device_bar_info = device.bars[device_bar];
	if((common_offset + sizeof(VirtioPciCommonCfg)) > common_bar_info.size
		|| (notify_offset + notify_length) > notify_bar_info.size
		|| (device_offset + sizeof(VirtioBlkConfig)) > device_bar_info.size)
	{
		debug("virtio-blk: capability points outside BAR")();
		return false;
	}

	auto &state = g_platform.virtio_blk_state;
	memset(&state, 0, sizeof(state));
	state.common_cfg = reinterpret_cast<volatile VirtioPciCommonCfg *>(common_bar_info.base + common_offset);
	state.device_cfg = reinterpret_cast<volatile VirtioBlkConfig *>(device_bar_info.base + device_offset);
	state.notify_multiplier = notify_multiplier;

	WriteDeviceStatus(state.common_cfg, 0);
	WriteDeviceStatus(state.common_cfg, kVirtioStatusAcknowledge);
	WriteDeviceStatus(state.common_cfg,
			static_cast<uint8_t>(state.common_cfg->device_status | kVirtioStatusDriver));

	state.common_cfg->device_feature_select = 0;
	const uint64_t device_features_low = state.common_cfg->device_feature;
	state.common_cfg->device_feature_select = 1;
	const uint64_t device_features_high = state.common_cfg->device_feature;
	const uint64_t device_features = device_features_low | (device_features_high << 32);
	if(0 == (device_features & kVirtioFeatureVersion1))
	{
		debug("virtio-blk: VERSION_1 feature missing")();
		return false;
	}

	state.common_cfg->driver_feature_select = 0;
	state.common_cfg->driver_feature = 0;
	state.common_cfg->driver_feature_select = 1;
	state.common_cfg->driver_feature = static_cast<uint32_t>(kVirtioFeatureVersion1 >> 32);
	WriteDeviceStatus(state.common_cfg,
			static_cast<uint8_t>(state.common_cfg->device_status | kVirtioStatusFeaturesOk));
	if(0 == (state.common_cfg->device_status & kVirtioStatusFeaturesOk))
	{
		debug("virtio-blk: feature negotiation rejected")();
		return false;
	}

	state.common_cfg->queue_select = 0;
	const uint16_t device_queue_size = state.common_cfg->queue_size;
	if(device_queue_size < 3u)
	{
		debug("virtio-blk: queue too small")();
		return false;
	}
	state.queue_size = MinU16(device_queue_size, kVirtioBlkQueueTargetSize);
	state.common_cfg->queue_size = state.queue_size;
	state.common_cfg->queue_msix_vector = kVirtioNoVector;

	if(!page_frames.Allocate(state.queue_memory, 3))
	{
		debug("virtio-blk: queue memory allocation failed")();
		return false;
	}
	memset(reinterpret_cast<void *>(state.queue_memory), 0, 3 * kPageSize);
	state.desc = reinterpret_cast<VirtqDesc *>(state.queue_memory);
	state.avail = reinterpret_cast<VirtqAvail *>(state.queue_memory + kPageSize);
	state.used = reinterpret_cast<volatile VirtqUsed *>(state.queue_memory + 2 * kPageSize);
	state.common_cfg->queue_desc = state.queue_memory;
	state.common_cfg->queue_driver = state.queue_memory + kPageSize;
	state.common_cfg->queue_device = state.queue_memory + 2 * kPageSize;

	const uint16_t queue_notify_off = state.common_cfg->queue_notify_off;
	const uint64_t notify_physical = notify_bar_info.base
			+ notify_offset
			+ static_cast<uint64_t>(queue_notify_off) * notify_multiplier;
	if((notify_physical + sizeof(uint16_t)) > (notify_bar_info.base + notify_bar_info.size))
	{
		debug("virtio-blk: notify register outside BAR")();
		return false;
	}
	state.notify_register = reinterpret_cast<volatile uint16_t *>(notify_physical);

	state.common_cfg->queue_enable = 1;
	if(!page_frames.Allocate(state.request_memory))
	{
		debug("virtio-blk: request memory allocation failed")();
		return false;
	}
	memset(reinterpret_cast<void *>(state.request_memory), 0, kPageSize);
	state.request_header = reinterpret_cast<VirtioBlkRequestHeader *>(state.request_memory);
	state.request_data = reinterpret_cast<uint8_t *>(state.request_memory + sizeof(VirtioBlkRequestHeader));
	state.request_status = reinterpret_cast<uint8_t *>(
			state.request_memory + sizeof(VirtioBlkRequestHeader) + kVirtioSectorSize);

	state.capacity_sectors = state.device_cfg->capacity;
	state.pci_index = static_cast<uint16_t>(device_index);
	state.last_used_idx = state.used->idx;
	WriteDeviceStatus(state.common_cfg,
			static_cast<uint8_t>(state.common_cfg->device_status | kVirtioStatusDriverOk));
	state.present = true;

	g_platform.virtio_blk_public.present = true;
	g_platform.virtio_blk_public.queue_size = state.queue_size;
	g_platform.virtio_blk_public.capacity_sectors = state.capacity_sectors;
	g_platform.virtio_blk_public.pci_index = static_cast<uint16_t>(device_index);

	debug("virtio-blk: ready pci=")(device.bus, 16, 2)
		(":")(device.slot, 16, 2)
		(".")(device.function, 16, 1)
		(" sectors=")(state.capacity_sectors)
		(" qsize=")(state.queue_size)();
	return true;
}

[[nodiscard]] bool ProbeDevices(VirtualMemory &kernel_vm)
{
	memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
	for(size_t i = 0; i < g_platform.device_count; ++i)
	{
		if(!ProbeVirtioBlkDevice(kernel_vm, i))
		{
			return false;
		}
	}
	if(!g_platform.virtio_blk_public.present)
	{
		debug("virtio-blk: no device present")();
	}
	return true;
}

[[nodiscard]] bool VirtioBlkReadSector(uint64_t sector, uint8_t *buffer, size_t length)
{
	auto &state = g_platform.virtio_blk_state;
	if(!state.present || (nullptr == buffer) || (length > kVirtioSectorSize))
	{
		return false;
	}
	if(sector >= state.capacity_sectors)
	{
		return false;
	}

	memset(state.request_header, 0, sizeof(*state.request_header));
	memset(state.request_data, 0, kVirtioSectorSize);
	*state.request_status = 0xFFu;

	state.request_header->type = kVirtioBlkRequestIn;
	state.request_header->sector = sector;

	state.desc[0].addr = reinterpret_cast<uint64_t>(state.request_header);
	state.desc[0].len = sizeof(VirtioBlkRequestHeader);
	state.desc[0].flags = kVirtqDescFlagNext;
	state.desc[0].next = 1;

	state.desc[1].addr = reinterpret_cast<uint64_t>(state.request_data);
	state.desc[1].len = kVirtioSectorSize;
	state.desc[1].flags = static_cast<uint16_t>(kVirtqDescFlagWrite | kVirtqDescFlagNext);
	state.desc[1].next = 2;

	state.desc[2].addr = reinterpret_cast<uint64_t>(state.request_status);
	state.desc[2].len = 1;
	state.desc[2].flags = kVirtqDescFlagWrite;
	state.desc[2].next = 0;

	const uint16_t slot = state.avail->idx % state.queue_size;
	state.avail->ring[slot] = 0;
	asm volatile("" : : : "memory");
	++state.avail->idx;
	asm volatile("" : : : "memory");
	*state.notify_register = 0;

	for(uint32_t spin = 0; spin < 1000000u; ++spin)
	{
		if(state.used->idx != state.last_used_idx)
		{
			state.last_used_idx = state.used->idx;
			if(0 != *state.request_status)
			{
				debug("virtio-blk: request failed status=")(*state.request_status)();
				return false;
			}
			memcpy(buffer, state.request_data, length);
			return true;
		}
		pause();
	}

	debug("virtio-blk: request timeout")();
	return false;
}

[[nodiscard]] bool VerifyVirtioBlkPrefix(uint64_t sector, const char *expected_prefix)
{
	uint8_t buffer[kVirtioSectorSize]{};
	if(!VirtioBlkReadSector(sector, buffer, sizeof(buffer)))
	{
		return false;
	}
	const size_t prefix_length = strlen(expected_prefix);
	if(0 != memcmp(buffer, expected_prefix, prefix_length))
	{
		debug("virtio-blk: sector verification failed sector=")(sector)();
		return false;
	}
	return true;
}

[[nodiscard]] bool RunVirtioBlkSmoke()
{
	if(!g_platform.virtio_blk_state.present)
	{
		return true;
	}
	if(g_platform.virtio_blk_state.capacity_sectors < 2u)
	{
		debug("virtio-blk: test disk too small")();
		return false;
	}
	if(!VerifyVirtioBlkPrefix(0, kVirtioSector0Prefix)
		|| !VerifyVirtioBlkPrefix(1, kVirtioSector1Prefix))
	{
		return false;
	}
	debug("virtio-blk smoke ok")();
	return true;
}

void ResetMpStateForFallback()
{
	ismp = 0;
	ncpu = 0;
	ioapicid = 0;
	ioapic = nullptr;
	lapic = nullptr;
}

[[nodiscard]] bool UseLegacyMpFallback(VirtualMemory &kernel_vm)
{
	ResetMpStateForFallback();
	mp_init();
	if(!ismp || (nullptr == lapic) || (nullptr == ioapic))
	{
		debug("platform: legacy MP fallback unavailable")();
		return false;
	}
	if(!MapIdentityRange(kernel_vm, reinterpret_cast<uint64_t>(lapic), kPageSize)
		|| !MapIdentityRange(kernel_vm, reinterpret_cast<uint64_t>(ioapic), kPageSize))
	{
		return false;
	}
	g_platform.used_legacy_mp_fallback = true;
	g_platform.acpi_active = false;
	g_platform.override_count = 0;
	if(!AddInterruptOverride(IRQ_TIMER, 2, 0))
	{
		return false;
	}
	ioapic_set_primary(0);
	g_platform.initialized = true;
	debug("platform: legacy MP fallback active")();
	return true;
}
}

bool platform_init(const BootInfo &boot_info, VirtualMemory &kernel_vm)
{
	memset(&g_platform, 0, sizeof(g_platform));

	uint64_t madt_physical = 0;
	uint64_t mcfg_physical = 0;
	const bool acpi_available = ResolveAcpiTables(kernel_vm, boot_info, madt_physical, mcfg_physical);
	if(!acpi_available)
	{
		if(BootSource::Limine == boot_info.source)
		{
			debug("platform: ACPI required on modern boot path")();
			return false;
		}
		return UseLegacyMpFallback(kernel_vm);
	}

	if(!ParseMadt(kernel_vm, madt_physical) || !ParseMcfg(kernel_vm, mcfg_physical))
	{
		if(BootSource::Limine == boot_info.source)
		{
			debug("platform: required ACPI tables invalid")();
			return false;
		}
		return UseLegacyMpFallback(kernel_vm);
	}

	if(!AllocateCpusFromTopology())
	{
		return false;
	}
	if(!MapIdentityRange(kernel_vm, g_platform.lapic_base, kPageSize))
	{
		return false;
	}
	for(size_t i = 0; i < g_platform.ioapic_count; ++i)
	{
		if(!MapIdentityRange(kernel_vm, g_platform.ioapics[i].address, kPageSize))
		{
			return false;
		}
	}

	g_platform.acpi_active = true;
	if(!EnumeratePci(kernel_vm) || !ProbeDevices(kernel_vm) || !RunVirtioBlkSmoke())
	{
		return false;
	}

	g_platform.initialized = true;
	return true;
}

bool platform_enable_isa_irq(int bus_irq, int irq)
{
	if((nullptr == ioapic) || !ismp)
	{
		return false;
	}
	if(irq < 0)
	{
		irq = bus_irq;
	}

	uint32_t global_irq = static_cast<uint32_t>(bus_irq);
	uint16_t flags = 0;
	for(size_t i = 0; i < g_platform.override_count; ++i)
	{
		if(g_platform.overrides[i].bus_irq == static_cast<uint8_t>(bus_irq))
		{
			global_irq = g_platform.overrides[i].global_irq;
			flags = g_platform.overrides[i].flags;
			break;
		}
	}

	return ioapic_enable_gsi(global_irq, irq, flags);
}

const VirtioBlkDevice *platform_virtio_blk()
{
	return g_platform.virtio_blk_public.present ? &g_platform.virtio_blk_public : nullptr;
}

size_t platform_pci_device_count()
{
	return g_platform.device_count;
}

const PciDevice *platform_pci_devices()
{
	return g_platform.devices;
}
