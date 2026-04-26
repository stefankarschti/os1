#include "platform/pci.h"

#include "debug.h"
#include "memory_layout.h"
#include "string.h"
#include "virtualmemory.h"

namespace
{
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
		PciDevice *devices,
		size_t &device_count,
		uint8_t bus,
		uint8_t slot,
		uint8_t function,
		uint64_t config_physical)
{
	if(device_count >= kPlatformMaxPciDevices)
	{
		debug("pci: device table full")();
		return false;
	}

	PciDevice &device = devices[device_count++];
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
}

bool EnumeratePci(VirtualMemory &kernel_vm,
		const PciEcamRegion *regions,
		size_t region_count,
		PciDevice *devices,
		size_t &device_count)
{
	if((nullptr == regions) || (nullptr == devices))
	{
		return false;
	}

	device_count = 0;
	for(size_t region_index = 0; region_index < region_count; ++region_index)
	{
		const PciEcamRegion &region = regions[region_index];
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
					if(!RecordPciDevice(region,
							devices,
							device_count,
							static_cast<uint8_t>(bus),
							slot,
							function,
							config_physical))
					{
						return false;
					}
				}
			}
		}
	}

	debug("pci: enumerated devices=")(device_count)();
	return true;
}