// CPU and interrupt-controller topology normalization from platform discovery
// records into the x86_64 CPU/APIC globals used by low-level startup code.
#pragma once

// allocate CPU records and publish LAPIC/IOAPIC addresses from ACPI topology.
bool allocate_cpus_from_topology();

// clear x86_64 MP/APIC globals before falling back to legacy MP discovery.
void reset_mp_state_for_fallback();
