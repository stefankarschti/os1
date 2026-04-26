#include "platform.h"

#include "bootinfo.h"
#include "cpu.h"
#include "debug.h"
#include "drivers/block/virtio_blk.h"
#include "interrupt.h"
#include "ioapic.h"
#include "lapic.h"
#include "memory_layout.h"
#include "mp.h"
#include "pageframe.h"
#include "platform/acpi.h"
#include "platform/pci.h"
#include "string.h"
#include "virtualmemory.h"

extern PageFrameContainer page_frames;

namespace
{
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
	const BlockDevice *block_device;
	VirtioBlkDevice virtio_blk_public;
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

[[nodiscard]] bool AddLegacyInterruptOverride(uint8_t bus_irq, uint32_t global_irq, uint16_t flags)
{
	if(g_platform.override_count >= kPlatformMaxInterruptOverrides)
	{
		debug("platform: interrupt override table full")();
		return false;
	}
	InterruptOverride &entry = g_platform.overrides[g_platform.override_count++];
	entry.bus_irq = bus_irq;
	entry.flags = flags;
	entry.global_irq = global_irq;
	return true;
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

[[nodiscard]] bool ProbeDevices(VirtualMemory &kernel_vm)
{
	memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
	for(size_t i = 0; i < g_platform.device_count; ++i)
	{
		if(!ProbeVirtioBlkDevice(kernel_vm, page_frames, g_platform.devices[i], i, g_platform.virtio_blk_public))
		{
			return false;
		}
	}
	if(!g_platform.virtio_blk_public.present)
	{
		debug("virtio-blk: no device present")();
	}
	g_platform.block_device = VirtioBlkBlockDevice();
	return RunVirtioBlkSmoke();
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
	if(!AddLegacyInterruptOverride(IRQ_TIMER, 2, 0))
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

	const bool acpi_available = DiscoverAcpiPlatform(kernel_vm,
			boot_info,
			g_platform.lapic_base,
			g_platform.cpus,
			g_platform.cpu_count,
			g_platform.ioapics,
			g_platform.ioapic_count,
			g_platform.overrides,
			g_platform.override_count,
			g_platform.ecam_regions,
			g_platform.ecam_region_count);
	if(!acpi_available)
	{
		if(BootSource::Limine == boot_info.source)
		{
			debug("platform: ACPI required on modern boot path")();
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
	if(!EnumeratePci(kernel_vm,
			g_platform.ecam_regions,
			g_platform.ecam_region_count,
			g_platform.devices,
			g_platform.device_count)
		|| !ProbeDevices(kernel_vm))
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

const BlockDevice *platform_block_device()
{
	return g_platform.block_device;
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
