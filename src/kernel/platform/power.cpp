// Deterministic suspend/resume sequencing for bound PCI drivers and ACPI
// companion power methods.
#include "platform/platform.hpp"

#include "drivers/bus/device.hpp"
#include "drivers/bus/driver_registry.hpp"
#include "freestanding/string.hpp"
#include "platform/acpi_aml.hpp"
#include "platform/state.hpp"

namespace
{
[[nodiscard]] const PciDriver* find_bound_pci_driver(const DeviceBinding& binding)
{
    if((DeviceBus::Pci != binding.id.bus) || (nullptr == binding.driver_name))
    {
        return nullptr;
    }

    for(size_t i = 0; i < pci_driver_count(); ++i)
    {
        const PciDriver* driver = pci_driver_at(i);
        if((nullptr != driver) && freestanding::strings_equal(driver->name, binding.driver_name))
        {
            return driver;
        }
    }
    return nullptr;
}

[[nodiscard]] const AcpiDeviceInfo* find_acpi_companion(const DeviceBinding& binding)
{
    if((DeviceBus::Pci != binding.id.bus) || (binding.id.index >= g_platform.device_count))
    {
        return nullptr;
    }

    const PciDevice& pci = g_platform.devices[binding.id.index];
    const uint64_t adr = (static_cast<uint64_t>(pci.slot) << 16) | pci.function;
    for(size_t i = 0; i < g_platform.acpi_device_count; ++i)
    {
        const AcpiDeviceInfo& device = g_platform.acpi_devices[i];
        if(device.active && (0 != (device.flags & kAcpiDeviceHasAdr)) && (device.bus_number == pci.bus) &&
           (device.adr == adr))
        {
            return &device;
        }
    }
    return nullptr;
}
}  // namespace

bool platform_suspend_devices()
{
    const DeviceBinding* bindings = device_bindings();
    const size_t binding_count = device_binding_count();
    for(size_t i = binding_count; i > 0; --i)
    {
        const DeviceBinding& binding = bindings[i - 1];
        if(!binding.active)
        {
            continue;
        }

        if(const PciDriver* driver = find_bound_pci_driver(binding))
        {
            if((nullptr != driver->suspend) && !driver->suspend(binding.id))
            {
                return false;
            }
            if(const AcpiDeviceInfo* companion = find_acpi_companion(binding))
            {
                if(!acpi_set_device_power_state(companion->path, AcpiPowerState::D3))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool platform_resume_devices()
{
    const DeviceBinding* bindings = device_bindings();
    const size_t binding_count = device_binding_count();
    for(size_t i = 0; i < binding_count; ++i)
    {
        const DeviceBinding& binding = bindings[i];
        if(!binding.active)
        {
            continue;
        }

        if(const PciDriver* driver = find_bound_pci_driver(binding))
        {
            if(const AcpiDeviceInfo* companion = find_acpi_companion(binding))
            {
                if(!acpi_set_device_power_state(companion->path, AcpiPowerState::D0))
                {
                    return false;
                }
            }
            if((nullptr != driver->resume) && !driver->resume(binding.id))
            {
                return false;
            }
        }
    }
    return true;
}