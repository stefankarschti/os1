#include "sync/smp.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "debug/debug.hpp"

bool kernel_on_bsp()
{
    if(nullptr == g_cpu_boot)
    {
        return true;
    }
    return 0 != cpu_on_boot();
}

[[noreturn]] void kassert_on_bsp_failed(const char* file, int line)
{
    debug(file)(":")(static_cast<uint64_t>(line))(" BSP-only assertion failed")();
    for(;;)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}
