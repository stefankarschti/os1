// HPET accessors and MMIO capability probing.
#include "platform/hpet.hpp"

#include <stdint.h>

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "platform/platform.hpp"
#include "platform/state.hpp"

namespace
{
constexpr uint64_t kHpetCapabilitiesOffset = 0x000;
constexpr uint64_t kHpetMainCounterOffset = 0x0F0;
}  // namespace

bool platform_hpet_initialize()
{
    if(!g_platform.hpet.present)
    {
        return true;
    }
    if(0 == g_platform.hpet.physical_address)
    {
        debug("hpet: missing physical address")();
        g_platform.hpet = {};
        return true;
    }

    const uint64_t capabilities =
        *kernel_physical_pointer<volatile uint64_t>(g_platform.hpet.physical_address +
                                                    kHpetCapabilitiesOffset);
    const uint32_t period = static_cast<uint32_t>(capabilities >> 32);
    if(0 == period)
    {
        debug("hpet: invalid capability register")();
        g_platform.hpet = {};
        return true;
    }

    g_platform.hpet.hardware_rev_id = static_cast<uint8_t>(capabilities & 0xFFu);
    g_platform.hpet.comparator_count = static_cast<uint8_t>(((capabilities >> 8) & 0x1Fu) + 1u);
    g_platform.hpet.counter_size_64bit = 0 != (capabilities & (1ull << 13));
    g_platform.hpet.legacy_replacement_capable = 0 != (capabilities & (1ull << 15));
    g_platform.hpet.pci_vendor_id = static_cast<uint16_t>((capabilities >> 16) & 0xFFFFu);
    g_platform.hpet.counter_clock_period_fs = period;
    debug("hpet: ready base=0x")(g_platform.hpet.physical_address, 16)(" timers=")(
        g_platform.hpet.comparator_count)(" period_fs=")(g_platform.hpet.counter_clock_period_fs)();
    return true;
}

const HpetInfo* platform_hpet()
{
    return g_platform.hpet.present ? &g_platform.hpet : nullptr;
}

bool platform_hpet_read_main_counter(uint64_t& counter_value)
{
    counter_value = 0;
    if(!g_platform.hpet.present || (0 == g_platform.hpet.physical_address))
    {
        return false;
    }

    if(g_platform.hpet.counter_size_64bit)
    {
        counter_value =
            *kernel_physical_pointer<volatile uint64_t>(g_platform.hpet.physical_address +
                                                        kHpetMainCounterOffset);
    }
    else
    {
        counter_value =
            *kernel_physical_pointer<volatile uint32_t>(g_platform.hpet.physical_address +
                                                        kHpetMainCounterOffset);
    }
    return true;
}