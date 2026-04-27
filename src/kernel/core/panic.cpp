// Minimal halt loop used after unrecoverable kernel failures.
#include "core/panic.hpp"

[[noreturn]] void halt_forever()
{
    for(;;)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}