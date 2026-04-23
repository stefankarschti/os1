#ifndef OS1_OBSERVE_H
#define OS1_OBSERVE_H

#include <stdint.h>

enum
{
	OS1_OBSERVE_ABI_VERSION = 1,
	OS1_OBSERVE_SYSTEM = 1,
	OS1_OBSERVE_PROCESSES = 2,
	OS1_OBSERVE_CPUS = 3,
	OS1_OBSERVE_PCI = 4,
	OS1_OBSERVE_INITRD = 5,
	OS1_OBSERVE_CONSOLE_NONE = 0,
	OS1_OBSERVE_CONSOLE_VGA = 1,
	OS1_OBSERVE_CONSOLE_FRAMEBUFFER = 2,
	OS1_OBSERVE_CONSOLE_SERIAL = 3,
	OS1_OBSERVE_PROCESS_FLAG_USER_MODE = 1u << 0,
	OS1_OBSERVE_CPU_FLAG_BSP = 1u << 0,
	OS1_OBSERVE_CPU_FLAG_BOOTED = 1u << 1,
	OS1_OBSERVE_BOOTLOADER_NAME_BYTES = 64,
	OS1_OBSERVE_PROCESS_NAME_BYTES = 32,
	OS1_OBSERVE_INITRD_PATH_BYTES = 64,
};

#pragma pack(push, 1)

struct Os1ObserveHeader
{
	uint32_t abi_version;
	uint32_t kind;
	uint32_t record_size;
	uint32_t record_count;
};

struct Os1ObserveSystemRecord
{
	uint32_t boot_source;
	uint32_t console_kind;
	uint64_t tick_count;
	uint64_t total_pages;
	uint64_t free_pages;
	uint32_t process_count;
	uint32_t runnable_thread_count;
	uint32_t cpu_count;
	uint32_t pci_device_count;
	uint32_t virtio_blk_present;
	uint32_t reserved0;
	uint64_t virtio_blk_capacity_sectors;
	char bootloader_name[OS1_OBSERVE_BOOTLOADER_NAME_BYTES];
};

struct Os1ObserveProcessRecord
{
	uint64_t pid;
	uint64_t tid;
	uint64_t cr3;
	uint32_t process_state;
	uint32_t thread_state;
	uint32_t flags;
	char name[OS1_OBSERVE_PROCESS_NAME_BYTES];
};

struct Os1ObserveCpuRecord
{
	uint32_t logical_index;
	uint32_t apic_id;
	uint32_t flags;
	uint64_t current_pid;
	uint64_t current_tid;
};

struct Os1ObservePciBar
{
	uint64_t base;
	uint64_t size;
	uint8_t type;
};

struct Os1ObservePciRecord
{
	uint16_t segment_group;
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t class_code;
	uint8_t subclass;
	uint8_t prog_if;
	uint8_t revision;
	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint8_t capability_pointer;
	uint8_t bar_count;
	struct Os1ObservePciBar bars[6];
};

struct Os1ObserveInitrdRecord
{
	char path[OS1_OBSERVE_INITRD_PATH_BYTES];
	uint64_t size;
};

#pragma pack(pop)

#endif
