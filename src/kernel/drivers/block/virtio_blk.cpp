#include "drivers/block/virtio_blk.h"

#include "debug.h"
#include "drivers/block/block_device.h"
#include "memory_layout.h"
#include "string.h"
#include "virtualmemory.h"
#include "x86.h"

namespace
{
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

constinit VirtioBlkState g_virtio_blk{};
BlockDevice g_virtio_blk_device{};

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

[[nodiscard]] uint8_t PciRead8(uint64_t config_physical, uint16_t offset)
{
	return *reinterpret_cast<volatile uint8_t *>(config_physical + offset);
}

[[nodiscard]] uint32_t PciRead32(uint64_t config_physical, uint16_t offset)
{
	return *reinterpret_cast<volatile uint32_t *>(config_physical + offset);
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

[[nodiscard]] bool VirtioBlkReadSector(uint64_t sector, uint8_t *buffer, size_t length)
{
	auto &state = g_virtio_blk;
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

bool ReadBlockDevice(BlockDevice &device, uint64_t sector, void *buffer, size_t sector_count)
{
	if((device.driver_state != &g_virtio_blk) || (nullptr == buffer) || (0 == sector_count))
	{
		return false;
	}

	auto *cursor = static_cast<uint8_t *>(buffer);
	for(size_t i = 0; i < sector_count; ++i)
	{
		if(!VirtioBlkReadSector(sector + i, cursor + i * kVirtioSectorSize, kVirtioSectorSize))
		{
			return false;
		}
	}
	return true;
}

bool WriteBlockDevice(BlockDevice &, uint64_t, const void *, size_t)
{
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
}

bool ProbeVirtioBlkDevice(VirtualMemory &kernel_vm,
		PageFrameContainer &frames,
		const PciDevice &device,
		size_t device_index,
		VirtioBlkDevice &public_device)
{
	if((device.vendor_id != kPciVendorVirtio) || (device.device_id != kPciDeviceVirtioBlkModern))
	{
		return true;
	}
	if(g_virtio_blk.present)
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

	auto &state = g_virtio_blk;
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

	if(!frames.Allocate(state.queue_memory, 3))
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
	if(!frames.Allocate(state.request_memory))
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

	public_device.present = true;
	public_device.queue_size = state.queue_size;
	public_device.capacity_sectors = state.capacity_sectors;
	public_device.pci_index = static_cast<uint16_t>(device_index);

	g_virtio_blk_device.name = "virtio-blk";
	g_virtio_blk_device.sector_count = state.capacity_sectors;
	g_virtio_blk_device.sector_size = static_cast<uint32_t>(kVirtioSectorSize);
	g_virtio_blk_device.driver_state = &g_virtio_blk;
	g_virtio_blk_device.read = ReadBlockDevice;
	g_virtio_blk_device.write = WriteBlockDevice;

	debug("virtio-blk: ready pci=")(device.bus, 16, 2)
		(":")(device.slot, 16, 2)
		(".")(device.function, 16, 1)
		(" sectors=")(state.capacity_sectors)
		(" qsize=")(state.queue_size)();
	return true;
}

const BlockDevice *VirtioBlkBlockDevice()
{
	return g_virtio_blk.present ? &g_virtio_blk_device : nullptr;
}

bool RunVirtioBlkSmoke()
{
	if(!g_virtio_blk.present)
	{
		return true;
	}
	if(g_virtio_blk.capacity_sectors < 2u)
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