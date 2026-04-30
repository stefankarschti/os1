// Static PCI driver registry for early bus probing.
#pragma once

#include <stddef.h>

#include "mm/page_frame.hpp"
#include "platform/types.hpp"

class VirtualMemory;

struct PciDriver
{
    const char* name = nullptr;
    bool (*probe)(VirtualMemory& kernel_vm,
                  PageFrameContainer& frames,
                  const PciDevice& device,
                  size_t device_index,
                  DeviceId id) = nullptr;
    void (*remove)(DeviceId id) = nullptr;
};

void driver_registry_reset();
bool driver_registry_add_pci_driver(const PciDriver& driver);
size_t pci_driver_count();
const PciDriver* pci_driver_at(size_t index);
