// Bound device records stored in platform state.
#include "drivers/bus/device.hpp"

#include "platform/state.hpp"
#include "util/memory.h"

namespace
{
[[nodiscard]] bool device_id_equal(DeviceId left, DeviceId right)
{
    return (left.bus == right.bus) && (left.index == right.index);
}
}  // namespace

bool device_binding_publish(DeviceId id,
                            uint16_t pci_index,
                            const char* driver_name,
                            void* driver_state)
{
    if(DeviceBinding* existing = device_binding_find(id))
    {
        existing->active = true;
        existing->pci_index = pci_index;
        existing->driver_name = driver_name;
        existing->driver_state = driver_state;
        existing->state = DeviceState::Bound;
        return true;
    }

    if(g_platform.device_binding_count >= kPlatformMaxDeviceBindings)
    {
        return false;
    }

    DeviceBinding& binding = g_platform.device_bindings[g_platform.device_binding_count++];
    memset(&binding, 0, sizeof(binding));
    binding.active = true;
    binding.id = id;
    binding.state = DeviceState::Bound;
    binding.pci_index = pci_index;
    binding.driver_name = driver_name;
    binding.driver_state = driver_state;
    return true;
}

bool device_binding_set_state(DeviceId id, DeviceState state)
{
    DeviceBinding* binding = device_binding_find(id);
    if(nullptr == binding)
    {
        return false;
    }
    binding->state = state;
    return true;
}

void device_binding_remove(DeviceId id)
{
    if(DeviceBinding* binding = device_binding_find(id))
    {
        binding->active = false;
        binding->state = DeviceState::Removed;
    }
}

DeviceBinding* device_binding_find(DeviceId id)
{
    for(size_t i = 0; i < g_platform.device_binding_count; ++i)
    {
        DeviceBinding& binding = g_platform.device_bindings[i];
        if(binding.active && device_id_equal(binding.id, id))
        {
            return &binding;
        }
    }
    return nullptr;
}

size_t device_binding_count()
{
    return g_platform.device_binding_count;
}

const DeviceBinding* device_bindings()
{
    return g_platform.device_bindings;
}
