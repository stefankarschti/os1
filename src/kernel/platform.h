#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>
#include <stdint.h>

class VirtualMemory;
struct BlockDevice;
struct BootInfo;

constexpr size_t kPlatformMaxCpus = 64;
constexpr size_t kPlatformMaxIoApics = 8;
constexpr size_t kPlatformMaxInterruptOverrides = 32;
constexpr size_t kPlatformMaxPciEcamRegions = 8;
constexpr size_t kPlatformMaxPciDevices = 256;

struct CpuInfo
{
	uint32_t apic_id;
	bool enabled;
	bool is_bsp;
};

struct IoApicInfo
{
	uint32_t id;
	uint32_t address;
	uint32_t gsi_base;
};

struct InterruptOverride
{
	uint8_t bus_irq;
	uint8_t reserved0;
	uint16_t flags;
	uint32_t global_irq;
};

struct PciEcamRegion
{
	uint64_t base_address;
	uint16_t segment_group;
	uint8_t bus_start;
	uint8_t bus_end;
};

enum class PciBarType : uint8_t
{
	Unused = 0,
	Mmio32 = 1,
	Mmio64 = 2,
	Io = 3,
};

struct PciBarInfo
{
	uint64_t base;
	uint64_t size;
	PciBarType type;
};

struct PciDevice
{
	uint16_t segment_group;
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
	uint8_t header_type;
	uint8_t class_code;
	uint8_t subclass;
	uint8_t prog_if;
	uint8_t revision;
	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint8_t capability_pointer;
	uint8_t bar_count;
	uint16_t vendor_id;
	uint16_t device_id;
	uint64_t config_physical;
	PciBarInfo bars[6];
};

struct VirtioBlkDevice
{
	bool present;
	uint16_t queue_size;
	uint16_t pci_index;
	uint64_t capacity_sectors;
};

bool platform_init(const BootInfo &boot_info, VirtualMemory &kernel_vm);
bool platform_enable_isa_irq(int bus_irq, int irq = -1);
const BlockDevice *platform_block_device();
const VirtioBlkDevice *platform_virtio_blk();
size_t platform_pci_device_count();
const PciDevice *platform_pci_devices();

#endif // PLATFORM_H
