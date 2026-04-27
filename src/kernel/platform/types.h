// Platform discovery types shared by ACPI parsing, PCI enumeration, drivers,
// observability, and interrupt routing.
#ifndef OS1_KERNEL_PLATFORM_TYPES_H
#define OS1_KERNEL_PLATFORM_TYPES_H

#include <stddef.h>
#include <stdint.h>

constexpr size_t kPlatformMaxCpus = 64;
constexpr size_t kPlatformMaxIoApics = 8;
constexpr size_t kPlatformMaxInterruptOverrides = 32;
constexpr size_t kPlatformMaxPciEcamRegions = 8;
constexpr size_t kPlatformMaxPciDevices = 256;

// Normalized CPU topology record derived from ACPI MADT or legacy MP tables.
struct CpuInfo
{
	uint32_t apic_id;
	bool enabled;
	bool is_bsp;
};

// IOAPIC discovery record with its MMIO base and GSI range base.
struct IoApicInfo
{
	uint32_t id;
	uint32_t address;
	uint32_t gsi_base;
};

// ISA IRQ override from ACPI MADT interrupt-source override entries.
struct InterruptOverride
{
	uint8_t bus_irq;
	uint8_t reserved0;
	uint16_t flags;
	uint32_t global_irq;
};

// PCI ECAM window published by ACPI MCFG.
struct PciEcamRegion
{
	uint64_t base_address;
	uint16_t segment_group;
	uint8_t bus_start;
	uint8_t bus_end;
};

// Decoded PCI BAR address-space kind.
enum class PciBarType : uint8_t
{
	Unused = 0,
	Mmio32 = 1,
	Mmio64 = 2,
	Io = 3,
};

// Sized PCI BAR descriptor produced by the ECAM enumerator.
struct PciBarInfo
{
	uint64_t base;
	uint64_t size;
	PciBarType type;
};

// Normalized PCI function record used by driver probes and observe output.
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

// Public summary of the currently bound virtio-blk device.
struct VirtioBlkDevice
{
	bool present;
	uint16_t queue_size;
	uint16_t pci_index;
	uint64_t capacity_sectors;
};

#endif // OS1_KERNEL_PLATFORM_TYPES_H