#pragma once

#include <stddef.h>
#include <stdint.h>

#include "handoff/boot_info.hpp"

class VirtualMemory;

bool acpica_initialize_tables(VirtualMemory& kernel_vm, const BootInfo& boot_info);
bool acpica_discover_tables(uint64_t& madt_physical,
							uint64_t& mcfg_physical,
							uint64_t& hpet_physical,
							uint64_t& fadt_physical,
							uint64_t* ssdt_physical,
							size_t ssdt_capacity,
							size_t& ssdt_count);
bool acpica_tables_initialized();
const char* acpica_last_status();
void acpica_reset_for_tests();