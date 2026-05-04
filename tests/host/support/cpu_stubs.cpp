#include "arch/x86_64/cpu/cpu.hpp"

cpu* g_cpu_boot = nullptr;
cpu* g_cpu_host_current = nullptr;

namespace
{
cpu g_host_cpu{};

struct HostCpuInitializer
{
    HostCpuInitializer()
    {
        g_host_cpu.self = &g_host_cpu;
        g_host_cpu.next = nullptr;
        g_host_cpu.id = 0;
        g_host_cpu.booted = 1;
        g_host_cpu.magic = CPU_MAGIC;
        g_cpu_boot = &g_host_cpu;
        g_cpu_host_current = &g_host_cpu;
    }
} g_host_cpu_initializer;
}  // namespace

void cpu_set_kernel_stack(uint64_t stack_top)
{
    if(nullptr != g_cpu_host_current)
    {
        g_cpu_host_current->tss.rsp0 = stack_top;
    }
}

extern "C" void kernel_thread_start()
{
}