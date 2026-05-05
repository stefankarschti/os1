// Console stream implementation shared by kernel status messages and syscall
// write plumbing.
#include "console/console.hpp"

#include "core/kernel_state.hpp"
#include "debug/debug.hpp"
#include "sync/smp.hpp"

namespace
{
Spinlock g_console_output_lock{"console-output"};
}

void write_console_bytes(const char* data, size_t length)
{
    if(nullptr == data)
    {
        return;
    }

    IrqSpinGuard guard(g_console_output_lock);
    for(size_t i = 0; i < length; ++i)
    {
        debug.write(data[i]);
        if(active_terminal)
        {
            active_terminal->write(data[i]);
        }
    }
}

void write_console_line(const char* text)
{
    if(nullptr == text)
    {
        return;
    }

    IrqSpinGuard guard(g_console_output_lock);
    debug.write_line(text);
    if(active_terminal)
    {
        active_terminal->write_line(text);
    }
}
