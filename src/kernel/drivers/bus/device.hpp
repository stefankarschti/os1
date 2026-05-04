// Bound device records published by the minimal driver core.
#pragma once

#include <stddef.h>

#include "platform/types.hpp"

bool device_binding_publish(DeviceId id,
                            uint16_t pci_index,
                            const char* driver_name,
                            void* driver_state);
bool device_binding_set_state(DeviceId id, DeviceState state);
void device_binding_remove(DeviceId id);
DeviceBinding* device_binding_find(DeviceId id);
size_t device_binding_count();
const DeviceBinding* device_bindings();
