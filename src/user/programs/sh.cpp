#include <os1/observe.h>
#include <stddef.h>
#include <stdint.h>

#include <os1/syscall.hpp>

namespace
{
constexpr size_t kShellLineBytes = 128;
constexpr size_t kShellMaxTokens = 16;
constexpr size_t kShellObserveBufferBytes = 64 * 1024;

alignas(16) uint8_t g_observe_buffer[kShellObserveBufferBytes];

size_t string_length(const char* text)
{
    size_t length = 0;
    while((nullptr != text) && text[length])
    {
        ++length;
    }
    return length;
}

void write_bytes(const char* data, size_t length)
{
    if((nullptr == data) || (0 == length))
    {
        return;
    }
    os1::user::write(1, data, length);
}

void write_string(const char* text)
{
    write_bytes(text, string_length(text));
}

void write_char(char c)
{
    write_bytes(&c, 1);
}

void write_unsigned_base(uint64_t value, uint32_t base, size_t minimum_digits)
{
    char digits[32];
    size_t index = 0;
    do
    {
        const uint32_t digit = static_cast<uint32_t>(value % base);
        digits[index++] =
            (digit < 10u) ? static_cast<char>('0' + digit) : static_cast<char>('a' + (digit - 10u));
        value /= base;
    } while(value > 0);

    while(index < minimum_digits)
    {
        digits[index++] = '0';
    }

    while(index > 0)
    {
        write_char(digits[--index]);
    }
}

void write_unsigned(uint64_t value)
{
    write_unsigned_base(value, 10, 1);
}

void write_signed(long value)
{
    if(value < 0)
    {
        write_char('-');
        write_unsigned(static_cast<uint64_t>(-value));
        return;
    }
    write_unsigned(static_cast<uint64_t>(value));
}

void write_hex(uint64_t value, size_t digits = 1)
{
    write_unsigned_base(value, 16, digits);
}

bool is_space(char c)
{
    return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

bool strings_equal(const char* lhs, const char* rhs)
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

size_t normalize_line(char* buffer, long count)
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

size_t tokenize(char* buffer, char* tokens[kShellMaxTokens])
{
    size_t count = 0;
    size_t index = 0;
    while(buffer[index])
    {
        while(buffer[index] && is_space(buffer[index]))
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
        while(buffer[index] && !is_space(buffer[index]))
        {
            ++index;
        }
    }
    return count;
}

void write_prompt()
{
    write_string("os1> ");
}

void write_observe_failure(const char* command)
{
    write_string("observe failed: ");
    write_string(command);
    write_char('\n');
}

const char* boot_source_name(uint32_t source)
{
    switch(source)
    {
        case 1:
            return "bios";
        case 2:
            return "limine";
        case 3:
            return "test";
        default:
            return "unknown";
    }
}

const char* console_kind_name(uint32_t kind)
{
    switch(kind)
    {
        case OS1_OBSERVE_CONSOLE_VGA:
            return "vga";
        case OS1_OBSERVE_CONSOLE_FRAMEBUFFER:
            return "framebuffer";
        case OS1_OBSERVE_CONSOLE_SERIAL:
            return "serial";
        default:
            return "none";
    }
}

const char* process_state_name(uint32_t state)
{
    switch(state)
    {
        case 1:
            return "ready";
        case 2:
            return "running";
        case 3:
            return "dying";
        case 4:
            return "zombie";
        default:
            return "free";
    }
}

const char* thread_state_name(uint32_t state)
{
    switch(state)
    {
        case 1:
            return "ready";
        case 2:
            return "running";
        case 3:
            return "blocked";
        case 4:
            return "dying";
        default:
            return "free";
    }
}

const char* pci_bar_type_name(uint8_t type)
{
    switch(type)
    {
        case 1:
            return "mmio32";
        case 2:
            return "mmio64";
        case 3:
            return "io";
        default:
            return "unused";
    }
}

void write_yes_no(bool value)
{
    write_string(value ? "yes" : "no");
}

template<typename Record>
bool observe(uint32_t kind, const Record*& records, uint32_t& record_count)
{
    records = nullptr;
    record_count = 0;

    const long bytes = os1::user::observe(kind, g_observe_buffer, sizeof(g_observe_buffer));
    if(bytes < static_cast<long>(sizeof(Os1ObserveHeader)))
    {
        return false;
    }

    const auto* header = reinterpret_cast<const Os1ObserveHeader*>(g_observe_buffer);
    if((header->abi_version != OS1_OBSERVE_ABI_VERSION) || (header->kind != kind) ||
       (header->record_size != sizeof(Record)))
    {
        return false;
    }

    const size_t payload_bytes =
        static_cast<size_t>(header->record_count) * static_cast<size_t>(header->record_size);
    const size_t total_bytes = sizeof(Os1ObserveHeader) + payload_bytes;
    if((total_bytes > sizeof(g_observe_buffer)) || (static_cast<size_t>(bytes) != total_bytes))
    {
        return false;
    }

    record_count = header->record_count;
    records = reinterpret_cast<const Record*>(g_observe_buffer + sizeof(Os1ObserveHeader));
    return true;
}

void run_help()
{
    write_string("help echo pid sys ps cpu pci initrd exec exit\n");
}

void run_echo(size_t argc, char* argv[kShellMaxTokens])
{
    write_string("echo:");
    for(size_t i = 1; i < argc; ++i)
    {
        write_char(' ');
        write_string(argv[i]);
    }
    write_char('\n');
}

void run_pid()
{
    write_string("pid: ");
    const long pid = os1::user::getpid();
    if(pid < 0)
    {
        write_string("error\n");
        return;
    }
    write_unsigned(static_cast<uint64_t>(pid));
    write_char('\n');
}

void run_sys()
{
    const Os1ObserveSystemRecord* records = nullptr;
    uint32_t record_count = 0;
    if(!observe(OS1_OBSERVE_SYSTEM, records, record_count) || (1u != record_count))
    {
        write_observe_failure("sys");
        return;
    }

    const Os1ObserveSystemRecord& record = records[0];
    write_string("sys boot=");
    write_string(boot_source_name(record.boot_source));
    write_string(" console=");
    write_string(console_kind_name(record.console_kind));
    write_string(" ticks=");
    write_unsigned(record.tick_count);
    write_string(" pages=");
    write_unsigned(record.free_pages);
    write_char('/');
    write_unsigned(record.total_pages);
    write_string(" procs=");
    write_unsigned(record.process_count);
    write_string(" runnable=");
    write_unsigned(record.runnable_thread_count);
    write_string(" cpus=");
    write_unsigned(record.cpu_count);
    write_string(" pci=");
    write_unsigned(record.pci_device_count);
    write_string(" virtio_blk=");
    write_yes_no(0 != record.virtio_blk_present);
    if(0 != record.virtio_blk_present)
    {
        write_string(" sectors=");
        write_unsigned(record.virtio_blk_capacity_sectors);
    }
    if(0 != record.bootloader_name[0])
    {
        write_string(" bootloader=");
        write_string(record.bootloader_name);
    }
    write_char('\n');
}

void run_ps()
{
    const Os1ObserveProcessRecord* records = nullptr;
    uint32_t record_count = 0;
    if(!observe(OS1_OBSERVE_PROCESSES, records, record_count))
    {
        write_observe_failure("ps");
        return;
    }

    write_string("pid tid pstate tstate mode cr3 name\n");
    for(uint32_t i = 0; i < record_count; ++i)
    {
        write_unsigned(records[i].pid);
        write_char(' ');
        write_unsigned(records[i].tid);
        write_char(' ');
        write_string(process_state_name(records[i].process_state));
        write_char(' ');
        write_string(thread_state_name(records[i].thread_state));
        write_char(' ');
        write_string((0 != (records[i].flags & OS1_OBSERVE_PROCESS_FLAG_USER_MODE)) ? "user"
                                                                                    : "kernel");
        write_string(" 0x");
        write_hex(records[i].cr3, 16);
        write_char(' ');
        write_string(records[i].name);
        write_char('\n');
    }
}

void run_cpu()
{
    const Os1ObserveCpuRecord* records = nullptr;
    uint32_t record_count = 0;
    if(!observe(OS1_OBSERVE_CPUS, records, record_count))
    {
        write_observe_failure("cpu");
        return;
    }

    write_string("cpu slot apic bsp booted pid tid\n");
    for(uint32_t i = 0; i < record_count; ++i)
    {
        write_string("cpu ");
        write_unsigned(records[i].logical_index);
        write_char(' ');
        write_hex(records[i].apic_id, 2);
        write_char(' ');
        write_yes_no(0 != (records[i].flags & OS1_OBSERVE_CPU_FLAG_BSP));
        write_char(' ');
        write_yes_no(0 != (records[i].flags & OS1_OBSERVE_CPU_FLAG_BOOTED));
        write_char(' ');
        write_unsigned(records[i].current_pid);
        write_char(' ');
        write_unsigned(records[i].current_tid);
        write_char('\n');
    }
}

void run_pci()
{
    const Os1ObservePciRecord* records = nullptr;
    uint32_t record_count = 0;
    if(!observe(OS1_OBSERVE_PCI, records, record_count))
    {
        write_observe_failure("pci");
        return;
    }

    write_string("pci bdf vendor:device class irq bars\n");
    for(uint32_t i = 0; i < record_count; ++i)
    {
        write_string("pci ");
        write_hex(records[i].bus, 2);
        write_char(':');
        write_hex(records[i].slot, 2);
        write_char('.');
        write_unsigned(records[i].function);
        write_char(' ');
        write_hex(records[i].vendor_id, 4);
        write_char(':');
        write_hex(records[i].device_id, 4);
        write_string(" class=");
        write_hex(records[i].class_code, 2);
        write_char(':');
        write_hex(records[i].subclass, 2);
        write_string(" irq=");
        write_unsigned(records[i].interrupt_line);
        write_char('/');
        write_unsigned(records[i].interrupt_pin);

        bool wrote_bar = false;
        for(uint8_t bar_index = 0; bar_index < records[i].bar_count && bar_index < 6; ++bar_index)
        {
            if((0 == records[i].bars[bar_index].base) || (0 == records[i].bars[bar_index].size))
            {
                continue;
            }
            write_string(wrote_bar ? "," : " bars=");
            write_string("bar");
            write_unsigned(bar_index);
            write_char(':');
            write_string(pci_bar_type_name(records[i].bars[bar_index].type));
            write_char('@');
            write_string("0x");
            write_hex(records[i].bars[bar_index].base, 1);
            write_char('+');
            write_string("0x");
            write_hex(records[i].bars[bar_index].size, 1);
            wrote_bar = true;
        }
        if(!wrote_bar)
        {
            write_string(" bars=none");
        }
        write_char('\n');
    }
}

void run_initrd()
{
    const Os1ObserveInitrdRecord* records = nullptr;
    uint32_t record_count = 0;
    if(!observe(OS1_OBSERVE_INITRD, records, record_count))
    {
        write_observe_failure("initrd");
        return;
    }

    write_string("initrd path size\n");
    for(uint32_t i = 0; i < record_count; ++i)
    {
        write_string("initrd ");
        write_string(records[i].path);
        write_string(" size=");
        write_unsigned(records[i].size);
        write_char('\n');
    }
}

void run_unknown(const char* command)
{
    write_string("unknown command: ");
    write_string(command);
    write_char('\n');
}

bool copy_string(char* destination, size_t destination_size, const char* source)
{
    if((nullptr == destination) || (0 == destination_size) || (nullptr == source))
    {
        return false;
    }

    size_t index = 0;
    while(source[index])
    {
        if((index + 1) >= destination_size)
        {
            return false;
        }
        destination[index] = source[index];
        ++index;
    }
    destination[index] = 0;
    return true;
}

bool resolve_command_path(const char* command, char* path, size_t path_size)
{
    if((nullptr == command) || (nullptr == path) || (0 == path_size))
    {
        return false;
    }

    if('/' == command[0])
    {
        return copy_string(path, path_size, command);
    }

    static constexpr char kBinPrefix[] = "/bin/";
    const size_t prefix_length = sizeof(kBinPrefix) - 1;
    const size_t command_length = string_length(command);
    if((prefix_length + command_length + 1) > path_size)
    {
        return false;
    }

    for(size_t i = 0; i < prefix_length; ++i)
    {
        path[i] = kBinPrefix[i];
    }
    for(size_t i = 0; i < command_length; ++i)
    {
        path[prefix_length + i] = command[i];
    }
    path[prefix_length + command_length] = 0;
    return true;
}

void write_spawn_outcome(const char* command, long pid, int status)
{
    write_string("shell spawn ");
    write_string(command);
    write_string(" ok pid=");
    write_signed(pid);
    write_string(" status=");
    write_signed(status);
    write_char('\n');
    write_string("shell prompt resumed\n");
}

void run_exec(size_t argc, char* argv[kShellMaxTokens])
{
    if(argc != 2)
    {
        write_string("usage: exec <path>\n");
        return;
    }

    char path[OS1_OBSERVE_INITRD_PATH_BYTES];
    if(!resolve_command_path(argv[1], path, sizeof(path)))
    {
        write_string("exec failed: ");
        write_string(argv[1]);
        write_char('\n');
        return;
    }

    write_string("shell exec start ");
    write_string(path);
    write_char('\n');
    const long result = os1::user::exec(path);
    write_string("shell exec returned ");
    write_signed(result);
    write_char('\n');
}

void run_external(const char* command)
{
    char path[OS1_OBSERVE_INITRD_PATH_BYTES];
    if(!resolve_command_path(command, path, sizeof(path)))
    {
        run_unknown(command);
        return;
    }

    const long pid = os1::user::spawn(path);
    if(pid < 0)
    {
        run_unknown(command);
        return;
    }

    int status = 0;
    const long waited = os1::user::waitpid(static_cast<uint64_t>(pid), &status);
    if(waited != pid)
    {
        write_string("wait failed: ");
        write_string(command);
        write_char('\n');
        return;
    }

    write_spawn_outcome(command, waited, status);
}
}  // namespace

int main(void)
{
    char line[kShellLineBytes];
    char* tokens[kShellMaxTokens];

    write_string("shell prompt ready\n");
    for(;;)
    {
        write_prompt();
        const long count = os1::user::read(0, line, sizeof(line));
        if(count <= 0)
        {
            write_string("shell read failed\n");
            os1::user::yield();
            continue;
        }

        for(size_t i = 0; i < kShellMaxTokens; ++i)
        {
            tokens[i] = nullptr;
        }
        normalize_line(line, count);
        const size_t argc = tokenize(line, tokens);
        if(0 == argc)
        {
            continue;
        }

        if(strings_equal(tokens[0], "help"))
        {
            run_help();
        }
        else if(strings_equal(tokens[0], "echo"))
        {
            run_echo(argc, tokens);
        }
        else if(strings_equal(tokens[0], "pid"))
        {
            run_pid();
        }
        else if(strings_equal(tokens[0], "sys"))
        {
            run_sys();
        }
        else if(strings_equal(tokens[0], "ps"))
        {
            run_ps();
        }
        else if(strings_equal(tokens[0], "cpu"))
        {
            run_cpu();
        }
        else if(strings_equal(tokens[0], "pci"))
        {
            run_pci();
        }
        else if(strings_equal(tokens[0], "initrd"))
        {
            run_initrd();
        }
        else if(strings_equal(tokens[0], "exec"))
        {
            run_exec(argc, tokens);
        }
        else if(strings_equal(tokens[0], "exit"))
        {
            return 0;
        }
        else
        {
            run_external(tokens[0]);
        }
    }
}
