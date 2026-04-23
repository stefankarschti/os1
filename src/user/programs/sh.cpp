#include <os1/observe.h>
#include <os1/syscall.h>

#include <stddef.h>
#include <stdint.h>

namespace
{
constexpr size_t kShellLineBytes = 128;
constexpr size_t kShellMaxTokens = 16;
constexpr size_t kShellObserveBufferBytes = 64 * 1024;

alignas(16) uint8_t g_observe_buffer[kShellObserveBufferBytes];

size_t StringLength(const char *text)
{
	size_t length = 0;
	while((nullptr != text) && text[length])
	{
		++length;
	}
	return length;
}

void WriteBytes(const char *data, size_t length)
{
	if((nullptr == data) || (0 == length))
	{
		return;
	}
	os1_write(1, data, length);
}

void WriteString(const char *text)
{
	WriteBytes(text, StringLength(text));
}

void WriteChar(char c)
{
	WriteBytes(&c, 1);
}

void WriteUnsignedBase(uint64_t value, uint32_t base, size_t minimum_digits)
{
	char digits[32];
	size_t index = 0;
	do
	{
		const uint32_t digit = static_cast<uint32_t>(value % base);
		digits[index++] = (digit < 10u)
			? static_cast<char>('0' + digit)
			: static_cast<char>('a' + (digit - 10u));
		value /= base;
	}
	while(value > 0);

	while(index < minimum_digits)
	{
		digits[index++] = '0';
	}

	while(index > 0)
	{
		WriteChar(digits[--index]);
	}
}

void WriteUnsigned(uint64_t value)
{
	WriteUnsignedBase(value, 10, 1);
}

void WriteHex(uint64_t value, size_t digits = 1)
{
	WriteUnsignedBase(value, 16, digits);
}

bool IsSpace(char c)
{
	return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

bool StringsEqual(const char *lhs, const char *rhs)
{
	if((nullptr == lhs) || (nullptr == rhs))
	{
		return false;
	}
	for(size_t index = 0;; ++index)
	{
		if(lhs[index] != rhs[index])
		{
			return false;
		}
		if(0 == lhs[index])
		{
			return true;
		}
	}
}

size_t NormalizeLine(char *buffer, long count)
{
	size_t length = (count > 0) ? static_cast<size_t>(count) : 0;
	if(length >= kShellLineBytes)
	{
		length = kShellLineBytes - 1;
	}
	while((length > 0) && (('\n' == buffer[length - 1]) || ('\r' == buffer[length - 1])))
	{
		--length;
	}
	buffer[length] = 0;
	return length;
}

size_t Tokenize(char *buffer, char *tokens[kShellMaxTokens])
{
	size_t count = 0;
	size_t index = 0;
	while(buffer[index])
	{
		while(buffer[index] && IsSpace(buffer[index]))
		{
			buffer[index++] = 0;
		}
		if(0 == buffer[index])
		{
			break;
		}
		if(count == kShellMaxTokens)
		{
			return count;
		}
		tokens[count++] = buffer + index;
		while(buffer[index] && !IsSpace(buffer[index]))
		{
			++index;
		}
	}
	return count;
}

void WritePrompt()
{
	WriteString("os1> ");
}

void WriteObserveFailure(const char *command)
{
	WriteString("observe failed: ");
	WriteString(command);
	WriteChar('\n');
}

const char *BootSourceName(uint32_t source)
{
	switch(source)
	{
	case 1: return "bios";
	case 2: return "limine";
	case 3: return "test";
	default: return "unknown";
	}
}

const char *ConsoleKindName(uint32_t kind)
{
	switch(kind)
	{
	case OS1_OBSERVE_CONSOLE_VGA: return "vga";
	case OS1_OBSERVE_CONSOLE_FRAMEBUFFER: return "framebuffer";
	case OS1_OBSERVE_CONSOLE_SERIAL: return "serial";
	default: return "none";
	}
}

const char *ProcessStateName(uint32_t state)
{
	switch(state)
	{
	case 1: return "ready";
	case 2: return "running";
	case 3: return "dying";
	default: return "free";
	}
}

const char *ThreadStateName(uint32_t state)
{
	switch(state)
	{
	case 1: return "ready";
	case 2: return "running";
	case 3: return "blocked";
	case 4: return "dying";
	default: return "free";
	}
}

const char *PciBarTypeName(uint8_t type)
{
	switch(type)
	{
	case 1: return "mmio32";
	case 2: return "mmio64";
	case 3: return "io";
	default: return "unused";
	}
}

void WriteYesNo(bool value)
{
	WriteString(value ? "yes" : "no");
}

template<typename Record>
bool Observe(uint32_t kind, const Record *&records, uint32_t &record_count)
{
	records = nullptr;
	record_count = 0;

	const long bytes = os1_observe(kind, g_observe_buffer, sizeof(g_observe_buffer));
	if(bytes < static_cast<long>(sizeof(Os1ObserveHeader)))
	{
		return false;
	}

	const auto *header = reinterpret_cast<const Os1ObserveHeader *>(g_observe_buffer);
	if((header->abi_version != OS1_OBSERVE_ABI_VERSION)
		|| (header->kind != kind)
		|| (header->record_size != sizeof(Record)))
	{
		return false;
	}

	const size_t payload_bytes = static_cast<size_t>(header->record_count) * static_cast<size_t>(header->record_size);
	const size_t total_bytes = sizeof(Os1ObserveHeader) + payload_bytes;
	if((total_bytes > sizeof(g_observe_buffer)) || (static_cast<size_t>(bytes) != total_bytes))
	{
		return false;
	}

	record_count = header->record_count;
	records = reinterpret_cast<const Record *>(g_observe_buffer + sizeof(Os1ObserveHeader));
	return true;
}

void RunHelp()
{
	WriteString("help echo pid sys ps cpu pci initrd exit\n");
}

void RunEcho(size_t argc, char *argv[kShellMaxTokens])
{
	WriteString("echo:");
	for(size_t i = 1; i < argc; ++i)
	{
		WriteChar(' ');
		WriteString(argv[i]);
	}
	WriteChar('\n');
}

void RunPid()
{
	WriteString("pid: ");
	const long pid = os1_getpid();
	if(pid < 0)
	{
		WriteString("error\n");
		return;
	}
	WriteUnsigned(static_cast<uint64_t>(pid));
	WriteChar('\n');
}

void RunSys()
{
	const Os1ObserveSystemRecord *records = nullptr;
	uint32_t record_count = 0;
	if(!Observe(OS1_OBSERVE_SYSTEM, records, record_count) || (1u != record_count))
	{
		WriteObserveFailure("sys");
		return;
	}

	const Os1ObserveSystemRecord &record = records[0];
	WriteString("sys boot=");
	WriteString(BootSourceName(record.boot_source));
	WriteString(" console=");
	WriteString(ConsoleKindName(record.console_kind));
	WriteString(" ticks=");
	WriteUnsigned(record.tick_count);
	WriteString(" pages=");
	WriteUnsigned(record.free_pages);
	WriteChar('/');
	WriteUnsigned(record.total_pages);
	WriteString(" procs=");
	WriteUnsigned(record.process_count);
	WriteString(" runnable=");
	WriteUnsigned(record.runnable_thread_count);
	WriteString(" cpus=");
	WriteUnsigned(record.cpu_count);
	WriteString(" pci=");
	WriteUnsigned(record.pci_device_count);
	WriteString(" virtio_blk=");
	WriteYesNo(0 != record.virtio_blk_present);
	if(0 != record.virtio_blk_present)
	{
		WriteString(" sectors=");
		WriteUnsigned(record.virtio_blk_capacity_sectors);
	}
	if(0 != record.bootloader_name[0])
	{
		WriteString(" bootloader=");
		WriteString(record.bootloader_name);
	}
	WriteChar('\n');
}

void RunPs()
{
	const Os1ObserveProcessRecord *records = nullptr;
	uint32_t record_count = 0;
	if(!Observe(OS1_OBSERVE_PROCESSES, records, record_count))
	{
		WriteObserveFailure("ps");
		return;
	}

	WriteString("pid tid pstate tstate mode cr3 name\n");
	for(uint32_t i = 0; i < record_count; ++i)
	{
		WriteUnsigned(records[i].pid);
		WriteChar(' ');
		WriteUnsigned(records[i].tid);
		WriteChar(' ');
		WriteString(ProcessStateName(records[i].process_state));
		WriteChar(' ');
		WriteString(ThreadStateName(records[i].thread_state));
		WriteChar(' ');
		WriteString((0 != (records[i].flags & OS1_OBSERVE_PROCESS_FLAG_USER_MODE)) ? "user" : "kernel");
		WriteString(" 0x");
		WriteHex(records[i].cr3, 16);
		WriteChar(' ');
		WriteString(records[i].name);
		WriteChar('\n');
	}
}

void RunCpu()
{
	const Os1ObserveCpuRecord *records = nullptr;
	uint32_t record_count = 0;
	if(!Observe(OS1_OBSERVE_CPUS, records, record_count))
	{
		WriteObserveFailure("cpu");
		return;
	}

	WriteString("cpu slot apic bsp booted pid tid\n");
	for(uint32_t i = 0; i < record_count; ++i)
	{
		WriteString("cpu ");
		WriteUnsigned(records[i].logical_index);
		WriteChar(' ');
		WriteHex(records[i].apic_id, 2);
		WriteChar(' ');
		WriteYesNo(0 != (records[i].flags & OS1_OBSERVE_CPU_FLAG_BSP));
		WriteChar(' ');
		WriteYesNo(0 != (records[i].flags & OS1_OBSERVE_CPU_FLAG_BOOTED));
		WriteChar(' ');
		WriteUnsigned(records[i].current_pid);
		WriteChar(' ');
		WriteUnsigned(records[i].current_tid);
		WriteChar('\n');
	}
}

void RunPci()
{
	const Os1ObservePciRecord *records = nullptr;
	uint32_t record_count = 0;
	if(!Observe(OS1_OBSERVE_PCI, records, record_count))
	{
		WriteObserveFailure("pci");
		return;
	}

	WriteString("pci bdf vendor:device class irq bars\n");
	for(uint32_t i = 0; i < record_count; ++i)
	{
		WriteString("pci ");
		WriteHex(records[i].bus, 2);
		WriteChar(':');
		WriteHex(records[i].slot, 2);
		WriteChar('.');
		WriteUnsigned(records[i].function);
		WriteChar(' ');
		WriteHex(records[i].vendor_id, 4);
		WriteChar(':');
		WriteHex(records[i].device_id, 4);
		WriteString(" class=");
		WriteHex(records[i].class_code, 2);
		WriteChar(':');
		WriteHex(records[i].subclass, 2);
		WriteString(" irq=");
		WriteUnsigned(records[i].interrupt_line);
		WriteChar('/');
		WriteUnsigned(records[i].interrupt_pin);

		bool wrote_bar = false;
		for(uint8_t bar_index = 0; bar_index < records[i].bar_count && bar_index < 6; ++bar_index)
		{
			if((0 == records[i].bars[bar_index].base) || (0 == records[i].bars[bar_index].size))
			{
				continue;
			}
			WriteString(wrote_bar ? "," : " bars=");
			WriteString("bar");
			WriteUnsigned(bar_index);
			WriteChar(':');
			WriteString(PciBarTypeName(records[i].bars[bar_index].type));
			WriteChar('@');
			WriteString("0x");
			WriteHex(records[i].bars[bar_index].base, 1);
			WriteChar('+');
			WriteString("0x");
			WriteHex(records[i].bars[bar_index].size, 1);
			wrote_bar = true;
		}
		if(!wrote_bar)
		{
			WriteString(" bars=none");
		}
		WriteChar('\n');
	}
}

void RunInitrd()
{
	const Os1ObserveInitrdRecord *records = nullptr;
	uint32_t record_count = 0;
	if(!Observe(OS1_OBSERVE_INITRD, records, record_count))
	{
		WriteObserveFailure("initrd");
		return;
	}

	WriteString("initrd path size\n");
	for(uint32_t i = 0; i < record_count; ++i)
	{
		WriteString("initrd ");
		WriteString(records[i].path);
		WriteString(" size=");
		WriteUnsigned(records[i].size);
		WriteChar('\n');
	}
}

void RunUnknown(const char *command)
{
	WriteString("unknown command: ");
	WriteString(command);
	WriteChar('\n');
}
}

int main(void)
{
	char line[kShellLineBytes];
	char *tokens[kShellMaxTokens];

	WriteString("shell prompt ready\n");
	for(;;)
	{
		WritePrompt();
		const long count = os1_read(0, line, sizeof(line));
		if(count <= 0)
		{
			WriteString("shell read failed\n");
			os1_yield();
			continue;
		}

		for(size_t i = 0; i < kShellMaxTokens; ++i)
		{
			tokens[i] = nullptr;
		}
		NormalizeLine(line, count);
		const size_t argc = Tokenize(line, tokens);
		if(0 == argc)
		{
			continue;
		}

		if(StringsEqual(tokens[0], "help"))
		{
			RunHelp();
		}
		else if(StringsEqual(tokens[0], "echo"))
		{
			RunEcho(argc, tokens);
		}
		else if(StringsEqual(tokens[0], "pid"))
		{
			RunPid();
		}
		else if(StringsEqual(tokens[0], "sys"))
		{
			RunSys();
		}
		else if(StringsEqual(tokens[0], "ps"))
		{
			RunPs();
		}
		else if(StringsEqual(tokens[0], "cpu"))
		{
			RunCpu();
		}
		else if(StringsEqual(tokens[0], "pci"))
		{
			RunPci();
		}
		else if(StringsEqual(tokens[0], "initrd"))
		{
			RunInitrd();
		}
		else if(StringsEqual(tokens[0], "exit"))
		{
			return 0;
		}
		else
		{
			RunUnknown(tokens[0]);
		}
	}
}
