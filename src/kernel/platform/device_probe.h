// Platform-level device probing policy. This is deliberately thin until a real
// bus/driver registry is justified by a second PCI device family.
#ifndef OS1_KERNEL_PLATFORM_DEVICE_PROBE_H
#define OS1_KERNEL_PLATFORM_DEVICE_PROBE_H

class VirtualMemory;

// Probe supported devices from the normalized PCI table and publish facades.
bool ProbeDevices(VirtualMemory &kernel_vm);

#endif // OS1_KERNEL_PLATFORM_DEVICE_PROBE_H