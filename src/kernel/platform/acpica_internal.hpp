#pragma once

#include <stdint.h>

#include "handoff/boot_info.hpp"

class VirtualMemory;

enum class AcpicaBootStage : uint8_t
{
    Inactive = 0,
    TableDiscovery,
    TablesReady,
    NamespaceReady,
    MethodEvaluationReady,
};

VirtualMemory* acpica_kernel_vm();
const BootInfo* acpica_boot_info();
bool acpica_context_active();
const char* acpica_boot_stage_name();