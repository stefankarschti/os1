// Idle-loop implementation used when no runnable user thread is available.
#include "sched/idle.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "console/console.hpp"

void kernel_idle_thread()
{
    static bool announced = false;
    if(cpu_on_boot() && !announced)
    {
        announced = true;
        write_console_line("idle thread online");
    }
    for(;;)
    {
        // The kernel has no deferred work queue yet, so the idle thread sleeps until
        // an interrupt returns control to the scheduler.
        asm volatile("sti; hlt");
    }
}
