// Scheduler timer source selection shared by IRQ dispatch and boot bring-up.
#include "core/timer_source.hpp"

#include "arch/x86_64/interrupt/interrupt.hpp"
#include "platform/irq_registry.hpp"

namespace
{
SchedulerTimerSource g_scheduler_timer_source = SchedulerTimerSource::Pit;
}

void timer_source_set_scheduler(SchedulerTimerSource source)
{
    g_scheduler_timer_source = source;
}

SchedulerTimerSource timer_source_scheduler()
{
    return g_scheduler_timer_source;
}

bool timer_vector_is_scheduler_tick(uint8_t vector)
{
    if(SchedulerTimerSource::Pit == g_scheduler_timer_source)
    {
        return IRQ_TIMER == legacy_irq_from_vector(vector);
    }

    const IrqRoute* route = platform_find_irq_route(vector);
    return (nullptr != route) && (IrqRouteKind::LocalApic == route->kind) &&
           (T_LTIMER == route->source_id);
}