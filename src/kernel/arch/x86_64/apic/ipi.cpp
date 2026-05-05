// Local APIC interrupt-command helpers for kernel IPIs.
#include "arch/x86_64/apic/ipi.hpp"

#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"
#include "arch/x86_64/interrupt/vector_allocator.hpp"
#include "debug/event_ring.hpp"
#include "sync/smp.hpp"

namespace
{
constexpr uint32_t kIpiAllButSelf = 0x000C0000u;

uint8_t g_reschedule_vector = 0;
bool g_reschedule_vector_initialized = false;

void wait_for_icr_delivery()
{
    while((lapic[ICRLO] & DELIVS) != 0u)
    {
#if defined(OS1_HOST_TEST)
        __atomic_signal_fence(__ATOMIC_ACQ_REL);
#else
        asm volatile("pause" ::: "memory");
#endif
    }
}

void lapic_icr_write(uint32_t high, uint32_t low)
{
    wait_for_icr_delivery();
    lapic[ICRHI] = high;
    lapic[ICRLO] = low;
    (void)lapic[ID];
    wait_for_icr_delivery();
}

[[nodiscard]] bool ipi_vector_valid(uint8_t vector)
{
    return interrupt_vector_is_external(vector);
}
}  // namespace

bool ipi_initialize()
{
    KASSERT_ON_BSP();
    if(g_reschedule_vector_initialized)
    {
        return irq_vector_is_allocated(g_reschedule_vector) || irq_reserve_vector(g_reschedule_vector);
    }

    uint8_t vector = 0;
    if(!irq_allocate_vector(vector))
    {
        return false;
    }

    g_reschedule_vector = vector;
    g_reschedule_vector_initialized = true;
    return true;
}

bool ipi_send(uint8_t apic_id, uint8_t vector)
{
    if((nullptr == lapic) || !ipi_vector_valid(vector))
    {
        return false;
    }

    lapic_icr_write(static_cast<uint32_t>(apic_id) << 24, vector);
    return true;
}

bool ipi_send_all_but_self(uint8_t vector)
{
    if((nullptr == lapic) || !ipi_vector_valid(vector))
    {
        return false;
    }

    lapic_icr_write(0, kIpiAllButSelf | vector);
    return true;
}

bool ipi_send_reschedule(const cpu* target)
{
    if((nullptr == target) || !g_reschedule_vector_initialized)
    {
        return false;
    }

    kernel_event::record(OS1_KERNEL_EVENT_IPI_RESCHED,
                         OS1_KERNEL_EVENT_FLAG_BEGIN,
                         cpu_cur()->id,
                         target->id,
                         g_reschedule_vector,
                         0);
    const bool sent = ipi_send(target->id, g_reschedule_vector);
    if(!sent)
    {
        kernel_event::record(OS1_KERNEL_EVENT_IPI_RESCHED,
                             OS1_KERNEL_EVENT_FLAG_FAILURE,
                             cpu_cur()->id,
                             target->id,
                             g_reschedule_vector,
                             0);
    }
    return sent;
}

uint8_t ipi_reschedule_vector()
{
    return g_reschedule_vector_initialized ? g_reschedule_vector : 0;
}

bool ipi_is_reschedule_vector(uint8_t vector)
{
    return g_reschedule_vector_initialized && (g_reschedule_vector == vector);
}
