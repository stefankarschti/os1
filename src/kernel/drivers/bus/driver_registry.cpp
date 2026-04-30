// Fixed-size early driver registry.
#include "drivers/bus/driver_registry.hpp"

#include "util/memory.h"

namespace
{
constexpr size_t kMaxPciDrivers = 16;

PciDriver g_pci_drivers[kMaxPciDrivers];
size_t g_pci_driver_count = 0;
}  // namespace

void driver_registry_reset()
{
    memset(g_pci_drivers, 0, sizeof(g_pci_drivers));
    g_pci_driver_count = 0;
}

bool driver_registry_add_pci_driver(const PciDriver& driver)
{
    if((nullptr == driver.name) || (nullptr == driver.probe) || (g_pci_driver_count >= kMaxPciDrivers))
    {
        return false;
    }
    g_pci_drivers[g_pci_driver_count++] = driver;
    return true;
}

size_t pci_driver_count()
{
    return g_pci_driver_count;
}

const PciDriver* pci_driver_at(size_t index)
{
    return (index < g_pci_driver_count) ? &g_pci_drivers[index] : nullptr;
}
