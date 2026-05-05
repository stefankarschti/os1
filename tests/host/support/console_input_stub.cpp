#include "console/console_input.hpp"

#include "sync/wait_queue.hpp"

Spinlock g_console_input_lock{"console-input"};

namespace
{
WaitQueue g_console_read_waiters{"console-read"};
}

void console_input_initialize()
{
    IrqSpinGuard guard(g_console_input_lock);
    g_console_read_waiters.lock.reset("console-read");
    g_console_read_waiters.head = nullptr;
    g_console_read_waiters.name = "console-read";
}

void console_input_on_keyboard_char(char)
{
}

void console_input_poll_serial()
{
}

bool console_input_has_line()
{
    return false;
}

bool console_input_pop_line(char*, size_t, size_t& line_length)
{
    line_length = 0;
    return false;
}

WaitQueue& console_input_read_wait_queue()
{
    return g_console_read_waiters;
}
