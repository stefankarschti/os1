// CPU and interrupt-controller topology normalization from platform discovery
// records into the x86_64 CPU/APIC globals used by low-level startup code.
#pragma once

#include <stdint.h>

struct ioapic;

extern int ismp;
extern int ncpu;
extern uint8_t ioapicid;
extern volatile struct ioapic* ioapic;

// allocate CPU records and publish LAPIC/IOAPIC addresses from ACPI topology.
bool allocate_cpus_from_topology();

// clear x86_64 CPU/APIC discovery globals before publishing ACPI topology.
void reset_topology_state();
