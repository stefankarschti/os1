// Generic IRQ flow around the current legacy PIC/APIC hybrid routing.
#include "core/irq_dispatch.h"

#include "arch/x86_64/apic/lapic.h"
#include "arch/x86_64/cpu/io_port.h"
#include "arch/x86_64/interrupt/interrupt.h"
#include "console/console_input.h"
#include "core/kernel_state.h"
#include "sched/scheduler.h"
#include "syscall/console_read.h"
#include "proc/thread.h"

namespace
{
void AcknowledgeLegacyIrq(int irq)
{
	lapic_eoi();
	outb(0x20, 0x20);
	if(irq >= 8)
	{
		outb(0xA0, 0x20);
	}
}
}

Thread *HandleIrq(TrapFrame *frame)
{
	const int irq = (int)(frame->vector - T_IRQ0);
	if(IRQ_KBD == irq)
	{
		DispatchIRQHook(irq);
	}
	else if(IRQ_TIMER == irq)
	{
		ConsoleInputPollSerial();
		++g_timer_ticks;
	}

	AcknowledgeLegacyIrq(irq);
	WakeConsoleReaders(page_frames);

	if(nullptr == currentThread())
	{
		return nullptr;
	}

	if(IRQ_TIMER == irq)
	{
		return ScheduleNext(true);
	}

	reapDeadThreads(page_frames);
	if((currentThread() == idleThread()) && (nullptr != firstRunnableUserThread()))
	{
		return ScheduleNext(true);
	}
	return currentThread();
}