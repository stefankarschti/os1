#include "syscall/observe.hpp"

#include <os1/observe.h>

#include "arch/x86_64/cpu/cpu.hpp"
#include "fs/initrd.hpp"
#include "mm/user_copy.hpp"
#include "arch/x86_64/apic/mp.hpp"
#include "platform/platform.hpp"
#include "proc/thread.hpp"
#include "util/fixed_string.hpp"

namespace
{
[[nodiscard]] size_t CountActiveProcesses()
{
	size_t count = 0;
	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		if(ProcessState::Free != processTable[i].state)
		{
			++count;
		}
	}
	return count;
}


[[nodiscard]] uint32_t ObserveConsoleKind(const TextDisplayBackend *text_display)
{
	if(nullptr == text_display)
	{
		return OS1_OBSERVE_CONSOLE_SERIAL;
	}

	switch(text_display->kind)
	{
	case TextDisplayBackendKind::VgaText:
		return OS1_OBSERVE_CONSOLE_VGA;
	case TextDisplayBackendKind::FramebufferText:
		return OS1_OBSERVE_CONSOLE_FRAMEBUFFER;
	default:
		return OS1_OBSERVE_CONSOLE_NONE;
	}
}

bool BeginObserveTransfer(const ObserveContext &context,
		Thread *thread,
		uint64_t user_buffer,
		size_t user_length,
		uint32_t kind,
		uint32_t record_size,
		uint32_t record_count,
		size_t &offset,
		long &result)
{
	offset = 0;
	result = -1;
	if((nullptr == context.frames) || (nullptr == thread) || (0 == user_buffer) || (0 == user_length))
	{
		return false;
	}

	const size_t payload_bytes = static_cast<size_t>(record_size) * static_cast<size_t>(record_count);
	if((0 != record_count) && ((payload_bytes / record_count) != record_size))
	{
		return false;
	}
	const size_t total_bytes = sizeof(Os1ObserveHeader) + payload_bytes;
	if(total_bytes < sizeof(Os1ObserveHeader) || (user_length < total_bytes))
	{
		return false;
	}

	const Os1ObserveHeader header{
		.abi_version = OS1_OBSERVE_ABI_VERSION,
		.kind = kind,
		.record_size = record_size,
		.record_count = record_count,
	};
	if(!copy_to_user(*context.frames, thread, user_buffer, &header, sizeof(header)))
	{
		return false;
	}

	offset = sizeof(header);
	result = static_cast<long>(total_bytes);
	return true;
}

bool WriteObserveRecord(const ObserveContext &context,
		Thread *thread,
		uint64_t user_buffer,
		size_t &offset,
		const void *record,
		size_t record_size)
{
	if((nullptr == context.frames) || (nullptr == thread) || (nullptr == record))
	{
		return false;
	}
	if(!copy_to_user(*context.frames, thread, user_buffer + offset, record, record_size))
	{
		return false;
	}
	offset += record_size;
	return true;
}

long SysObserveSystem(const ObserveContext &context, Thread *thread, uint64_t user_buffer, size_t length)
{
	size_t offset = 0;
	long result = -1;
	if(!BeginObserveTransfer(context,
			thread,
			user_buffer,
			length,
			OS1_OBSERVE_SYSTEM,
			sizeof(Os1ObserveSystemRecord),
			1,
			offset,
			result))
	{
		return -1;
	}

	Os1ObserveSystemRecord record{};
	record.boot_source = context.boot_info ? static_cast<uint32_t>(context.boot_info->source) : 0;
	record.console_kind = ObserveConsoleKind(context.text_display);
	record.tick_count = context.timer_ticks;
	record.total_pages = context.frames ? context.frames->page_count() : 0;
	record.free_pages = context.frames ? context.frames->free_page_count() : 0;
	record.process_count = static_cast<uint32_t>(CountActiveProcesses());
	record.runnable_thread_count = static_cast<uint32_t>(runnable_thread_count());
	record.cpu_count = static_cast<uint32_t>(ncpu);
	record.pci_device_count = static_cast<uint32_t>(platform_pci_device_count());
	const VirtioBlkDevice *virtio_blk = platform_virtio_blk();
	record.virtio_blk_present = (nullptr != virtio_blk) ? 1u : 0u;
	record.virtio_blk_capacity_sectors = (nullptr != virtio_blk) ? virtio_blk->capacity_sectors : 0;
	copy_fixed_string(record.bootloader_name,
		sizeof(record.bootloader_name),
		(context.boot_info && context.boot_info->bootloader_name) ? context.boot_info->bootloader_name : "");
	return WriteObserveRecord(context, thread, user_buffer, offset, &record, sizeof(record)) ? result : -1;
}

long SysObserveProcesses(const ObserveContext &context, Thread *thread, uint64_t user_buffer, size_t length)
{
	uint32_t record_count = 0;
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if((ThreadState::Free != threadTable[i].state) && (nullptr != threadTable[i].process))
		{
			++record_count;
		}
	}

	size_t offset = 0;
	long result = -1;
	if(!BeginObserveTransfer(context,
			thread,
			user_buffer,
			length,
			OS1_OBSERVE_PROCESSES,
			sizeof(Os1ObserveProcessRecord),
			record_count,
			offset,
			result))
	{
		return -1;
	}

	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		const Thread &entry = threadTable[i];
		if((ThreadState::Free == entry.state) || (nullptr == entry.process))
		{
			continue;
		}

		Os1ObserveProcessRecord record{};
		record.pid = entry.process->pid;
		record.tid = entry.tid;
		record.cr3 = entry.address_space_cr3;
		record.process_state = static_cast<uint32_t>(entry.process->state);
		record.thread_state = static_cast<uint32_t>(entry.state);
		record.flags = entry.user_mode ? static_cast<uint32_t>(OS1_OBSERVE_PROCESS_FLAG_USER_MODE) : 0u;
		copy_fixed_string(record.name, sizeof(record.name), entry.process->name);
		if(!WriteObserveRecord(context, thread, user_buffer, offset, &record, sizeof(record)))
		{
			return -1;
		}
	}

	return result;
}

long SysObserveCpus(const ObserveContext &context, Thread *thread, uint64_t user_buffer, size_t length)
{
	uint32_t record_count = 0;
	for(cpu *entry = g_cpu_boot; nullptr != entry; entry = entry->next)
	{
		++record_count;
	}

	size_t offset = 0;
	long result = -1;
	if(!BeginObserveTransfer(context,
			thread,
			user_buffer,
			length,
			OS1_OBSERVE_CPUS,
			sizeof(Os1ObserveCpuRecord),
			record_count,
			offset,
			result))
	{
		return -1;
	}

	uint32_t logical_index = 0;
	for(cpu *entry = g_cpu_boot; nullptr != entry; entry = entry->next, ++logical_index)
	{
		Os1ObserveCpuRecord record{};
		record.logical_index = logical_index;
		record.apic_id = entry->id;
		record.flags = (entry == g_cpu_boot) ? static_cast<uint32_t>(OS1_OBSERVE_CPU_FLAG_BSP) : 0u;
		if((entry == g_cpu_boot) || (0 != entry->booted))
		{
			record.flags |= static_cast<uint32_t>(OS1_OBSERVE_CPU_FLAG_BOOTED);
		}
		if((nullptr != entry->current_thread) && (nullptr != entry->current_thread->process))
		{
			record.current_pid = entry->current_thread->process->pid;
			record.current_tid = entry->current_thread->tid;
		}
		if(!WriteObserveRecord(context, thread, user_buffer, offset, &record, sizeof(record)))
		{
			return -1;
		}
	}

	return result;
}

long SysObservePci(const ObserveContext &context, Thread *thread, uint64_t user_buffer, size_t length)
{
	const size_t device_count = platform_pci_device_count();
	const PciDevice *devices = platform_pci_devices();

	size_t offset = 0;
	long result = -1;
	if(!BeginObserveTransfer(context,
			thread,
			user_buffer,
			length,
			OS1_OBSERVE_PCI,
			sizeof(Os1ObservePciRecord),
			static_cast<uint32_t>(device_count),
			offset,
			result))
	{
		return -1;
	}

	for(size_t i = 0; i < device_count; ++i)
	{
		Os1ObservePciRecord record{};
		record.segment_group = devices[i].segment_group;
		record.bus = devices[i].bus;
		record.slot = devices[i].slot;
		record.function = devices[i].function;
		record.vendor_id = devices[i].vendor_id;
		record.device_id = devices[i].device_id;
		record.class_code = devices[i].class_code;
		record.subclass = devices[i].subclass;
		record.prog_if = devices[i].prog_if;
		record.revision = devices[i].revision;
		record.interrupt_line = devices[i].interrupt_line;
		record.interrupt_pin = devices[i].interrupt_pin;
		record.capability_pointer = devices[i].capability_pointer;
		record.bar_count = devices[i].bar_count;
		for(size_t bar_index = 0; bar_index < 6; ++bar_index)
		{
			record.bars[bar_index].base = devices[i].bars[bar_index].base;
			record.bars[bar_index].size = devices[i].bars[bar_index].size;
			record.bars[bar_index].type = static_cast<uint8_t>(devices[i].bars[bar_index].type);
		}
		if(!WriteObserveRecord(context, thread, user_buffer, offset, &record, sizeof(record)))
		{
			return -1;
		}
	}

	return result;
}

struct InitrdCountContext
{
	uint32_t count = 0;
};

bool CountInitrdRecord(const char *, const uint8_t *, uint64_t, void *context)
{
	auto *count = static_cast<InitrdCountContext *>(context);
	if(nullptr == count)
	{
		return false;
	}
	++count->count;
	return true;
}

struct InitrdWriteContext
{
	const ObserveContext *observe_context = nullptr;
	Thread *thread = nullptr;
	uint64_t user_buffer = 0;
	size_t offset = 0;
};

bool WriteInitrdRecordCallback(const char *archive_name, const uint8_t *, uint64_t file_size, void *context)
{
	auto *write = static_cast<InitrdWriteContext *>(context);
	if((nullptr == write) || (nullptr == write->observe_context))
	{
		return false;
	}

	Os1ObserveInitrdRecord record{};
	copy_initrd_path(record.path, sizeof(record.path), archive_name);
	record.size = file_size;
	return WriteObserveRecord(*write->observe_context,
		write->thread,
		write->user_buffer,
		write->offset,
		&record,
		sizeof(record));
}

long SysObserveInitrd(const ObserveContext &context, Thread *thread, uint64_t user_buffer, size_t length)
{
	InitrdCountContext count{};
	if((nullptr != context.boot_info) && (context.boot_info->module_count > 0))
	{
		if(!for_each_initrd_file(CountInitrdRecord, &count))
		{
			return -1;
		}
	}

	size_t offset = 0;
	long result = -1;
	if(!BeginObserveTransfer(context,
			thread,
			user_buffer,
			length,
			OS1_OBSERVE_INITRD,
			sizeof(Os1ObserveInitrdRecord),
			count.count,
			offset,
			result))
	{
		return -1;
	}

	if(0 == count.count)
	{
		return result;
	}

	InitrdWriteContext write{
		.observe_context = &context,
		.thread = thread,
		.user_buffer = user_buffer,
		.offset = offset,
	};
	if(!for_each_initrd_file(WriteInitrdRecordCallback, &write))
	{
		return -1;
	}
	return result;
}
}

long sys_observe(const ObserveContext &context, uint64_t kind, uint64_t user_buffer, size_t length)
{
	Thread *thread = current_thread();
	if(nullptr == thread)
	{
		return -1;
	}

	switch(kind)
	{
	case OS1_OBSERVE_SYSTEM:
		return SysObserveSystem(context, thread, user_buffer, length);
	case OS1_OBSERVE_PROCESSES:
		return SysObserveProcesses(context, thread, user_buffer, length);
	case OS1_OBSERVE_CPUS:
		return SysObserveCpus(context, thread, user_buffer, length);
	case OS1_OBSERVE_PCI:
		return SysObservePci(context, thread, user_buffer, length);
	case OS1_OBSERVE_INITRD:
		return SysObserveInitrd(context, thread, user_buffer, length);
	default:
		return -1;
	}
}