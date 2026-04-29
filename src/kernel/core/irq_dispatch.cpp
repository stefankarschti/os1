// Generic IRQ flow around the current legacy PIC/APIC hybrid routing.
#include "core/irq_dispatch.hpp"

#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/cpu/io_port.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "console/console_input.hpp"
#include "core/kernel_state.hpp"
#include "debug/event_ring.hpp"
#include "proc/thread.hpp"
#include "sched/scheduler.hpp"
#include "syscall/console_read.hpp"

namespace
{
void acknowledge_irq_vector(uint8_t vector)
{
    lapic_eoi();
    const int irq = legacy_irq_from_vector(vector);
    if(irq < 0)
    {
        return;
    }
    outb(0x20, 0x20);
    if(irq >= 8)
    {
        outb(0xA0, 0x20);
    }
}
}  // namespace

Thread* handle_irq(TrapFrame* frame)
{
    const uint8_t vector = static_cast<uint8_t>(frame->vector);
    const int irq = legacy_irq_from_vector(vector);
    if(IRQ_TIMER != irq)
    {
        kernel_event::record(OS1_KERNEL_EVENT_IRQ,
                             0,
                             (irq >= 0) ? static_cast<uint64_t>(irq) : static_cast<uint64_t>(vector),
                             frame->vector,
                             g_timer_ticks,
                             0);
    }
    if(IRQ_TIMER == irq)
    {
        console_input_poll_serial();
        ++g_timer_ticks;
    }
    dispatch_interrupt_vector(vector);

    acknowledge_irq_vector(vector);
    wake_console_readers(page_frames);

    if(nullptr == current_thread())
    {
        return nullptr;
    }

    if(IRQ_TIMER == irq)
    {
        return schedule_next(true);
    }

    reap_dead_threads(page_frames);
    if((current_thread() == idle_thread()) && (nullptr != first_runnable_user_thread()))
    {
        return schedule_next(true);
    }
    return current_thread();
}
