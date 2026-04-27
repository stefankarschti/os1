// Legacy BIOS MP-table fallback for the compatibility boot path.
#ifndef OS1_KERNEL_PLATFORM_LEGACY_MP_H
#define OS1_KERNEL_PLATFORM_LEGACY_MP_H

class VirtualMemory;

// Use the x86 MP table path when ACPI discovery is unavailable on BIOS boots.
bool UseLegacyMpFallback(VirtualMemory &kernel_vm);

#endif // OS1_KERNEL_PLATFORM_LEGACY_MP_H