// Legacy BIOS MP-table fallback for the compatibility boot path.
#pragma once

class VirtualMemory;

// Use the x86 MP table path when ACPI discovery is unavailable on BIOS boots.
bool use_legacy_mp_fallback(VirtualMemory &kernel_vm);

