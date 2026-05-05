// Local APIC interrupt-command helpers for kernel IPIs.
#include "arch/x86_64/apic/ipi.hpp"

#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/interrupt/interrupt.hpp"

namespace
{
constexpr uint32_t kIpiAllButSelf = 0x000C0000u;

void wait_for_icr_delivery()
{
    while((lapic[ICRLO] & DELIVS) != 0u)
    {
        asm volatile("pause" ::: "memory");
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
