// C trap entry reached from architecture assembly after a TrapFrame has been
// saved on the current kernel stack.
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "core/fault.hpp"
#include "core/irq_dispatch.hpp"
#include "syscall/dispatch.hpp"

extern "C" Thread* trap_dispatch(TrapFrame* frame)
{
    if((nullptr == frame) || (frame->vector > 255))
    {
        return nullptr;
    }

    if((frame->vector >= T_IRQ0) && (frame->vector < (T_IRQ0 + 16)))
    {
        return HandleIrq(frame);
    }
    if(frame->vector == T_SYSCALL)
    {
        return handle_syscall(frame);
    }
    return HandleException(frame);
}