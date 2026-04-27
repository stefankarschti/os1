// CPU and interrupt-controller topology normalization from platform discovery
// records into the x86_64 CPU/APIC globals used by low-level startup code.
#ifndef OS1_KERNEL_PLATFORM_TOPOLOGY_H
#define OS1_KERNEL_PLATFORM_TOPOLOGY_H

// Allocate CPU records and publish LAPIC/IOAPIC addresses from ACPI topology.
bool AllocateCpusFromTopology();

// Clear x86_64 MP/APIC globals before falling back to legacy MP discovery.
void ResetMpStateForFallback();

#endif // OS1_KERNEL_PLATFORM_TOPOLOGY_H