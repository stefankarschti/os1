// Platform topology normalization for CPU and APIC structures.
#include "platform/topology.hpp"

#include "arch/x86_64/apic/ioapic.hpp"
#include "arch/x86_64/apic/lapic.hpp"
#include "arch/x86_64/apic/mp.hpp"
#include "arch/x86_64/cpu/cpu.hpp"
#include "debug/debug.hpp"
#include "platform/state.hpp"

bool allocate_cpus_from_topology()
{
    ismp = 1;
    ncpu = 0;
    ioapicid = 0;
    ioapic = nullptr;

    for(size_t i = 0; i < g_platform.cpu_count; ++i)
    {
        const CpuInfo& cpu_info = g_platform.cpus[i];
        if(!cpu_info.enabled)
        {
            continue;
        }
        if(cpu_info.apic_id > 0xFFu)
        {
            debug("acpi: APIC ID exceeds current cpu::id width")();
            return false;
        }
        debug("acpi: CPU apic_id=")(cpu_info.apic_id)(" bsp=")(cpu_info.is_bsp ? 1 : 0)();

        cpu* entry = cpu_info.is_bsp ? g_cpu_boot : cpu_alloc();
        if(nullptr == entry)
        {
            return false;
        }
        entry->id = static_cast<uint8_t>(cpu_info.apic_id);
        ++ncpu;
    }

    const IoApicInfo& primary = g_platform.ioapics[0];
    debug("acpi: primary ioapic id=")(primary.id)(" addr=0x")(primary.address,
                                                              16)(" gsi_base=")(primary.gsi_base)();
    ioapicid = static_cast<uint8_t>(primary.id);
    ioapic = reinterpret_cast<volatile struct ioapic*>(static_cast<uint64_t>(primary.address));
    lapic = reinterpret_cast<volatile uint32_t*>(g_platform.lapic_base);
    ioapic_set_primary(primary.gsi_base);
    return true;
}

void reset_mp_state_for_fallback()
{
    ismp = 0;
    ncpu = 0;
    ioapicid = 0;
    ioapic = nullptr;
    lapic = nullptr;
}