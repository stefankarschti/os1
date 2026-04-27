// C trap entry reached from architecture assembly after a TrapFrame has been
// saved on the current kernel stack.
#include "arch/x86_64/interrupt/interrupt.h"
#include "core/fault.h"
#include "core/irq_dispatch.h"
#include "syscall/dispatch.h"

extern "C" Thread *trap_dispatch(TrapFrame *frame)
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
		return HandleSyscall(frame);
	}
	return HandleException(frame);
}