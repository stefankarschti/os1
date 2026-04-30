// Virtio block driver. Platform discovery passes candidate PCI functions here;
// the driver owns queue setup, feature negotiation, smoke I/O, and publication
// of a generic BlockDevice facade.
#pragma once

#include <stddef.h>

#include "drivers/bus/driver_registry.hpp"
#include "mm/page_frame.hpp"
#include "platform/platform.hpp"

struct BlockDevice;
struct Process;
struct Thread;

class VirtualMemory;

// Try to bind one PCI function as virtio-blk and publish public device metadata.
bool probe_virtio_blk_device(VirtualMemory& kernel_vm,
                             PageFrameContainer& frames,
                             const PciDevice& device,
                             size_t device_index,
                             VirtioBlkDevice& public_device);
bool probe_virtio_blk_pci_driver(VirtualMemory& kernel_vm,
                                 PageFrameContainer& frames,
                                 const PciDevice& device,
                                 size_t device_index,
                                 DeviceId id);
void remove_virtio_blk_device(DeviceId id);
const PciDriver& virtio_blk_pci_driver();
// Run the current read-only sector smoke check against the bound device.
bool run_virtio_blk_smoke();
// Create a post-scheduler kernel-threaded smoke so sync wrappers exercise the
// threaded completion path after multitasking begins.
Thread* start_virtio_blk_threaded_smoke(Process* kernel_process, PageFrameContainer& frames);
// Return the generic block facade for filesystem/storage code.
const BlockDevice* virtio_blk_block_device();
