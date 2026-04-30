// PCI bus probing through the static driver registry.
#pragma once

#include "mm/page_frame.hpp"
#include "platform/types.hpp"

class VirtualMemory;

bool pci_bus_probe_all(VirtualMemory& kernel_vm, PageFrameContainer& frames);
bool pci_bus_remove_device(DeviceId id);
