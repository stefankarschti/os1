#pragma once

#include "handoff/boot_info.hpp"

class VirtualMemory;

bool acpica_initialize_tables(VirtualMemory& kernel_vm, const BootInfo& boot_info);
bool acpica_tables_initialized();
const char* acpica_last_status();
void acpica_reset_for_tests();