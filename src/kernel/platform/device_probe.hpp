// Platform-level device probing policy. This is deliberately thin until a real
// bus/driver registry is justified by a second PCI device family.
#pragma once

class VirtualMemory;

// Probe supported devices from the normalized PCI table and publish facades.
bool probe_devices(VirtualMemory& kernel_vm);
