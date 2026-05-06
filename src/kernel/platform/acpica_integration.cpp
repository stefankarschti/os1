#include "platform/acpica_integration.hpp"

#include <stddef.h>
#include <stdint.h>

extern "C"
{
#include "acpi.h"
#include "aclocal.h"
#include "acobject.h"
#include "actables.h"
#include "acglobal.h"
}

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "platform/acpica_internal.hpp"

namespace
{
constexpr const char* kAcpicaPinnedRelease = "20260408";
constexpr const char* kAcpicaPinnedShaShort = "232ff3f8ae1a";
constexpr const char* kStatusOk = "AE_OK";
constexpr UINT32 kInitialTableCapacity = 32;

struct [[gnu::packed]] AcpicaRsdp
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct AcpicaState
{
    VirtualMemory* kernel_vm = nullptr;
    const BootInfo* boot_info = nullptr;
    const char* last_status = kStatusOk;
    AcpicaBootStage stage = AcpicaBootStage::Inactive;
    bool tables_initialized = false;
};

AcpicaState g_acpica_state{};
ACPI_TABLE_DESC g_acpica_initial_tables[kInitialTableCapacity]{};

void prepare_early_mutex_state()
{
    for(UINT32 index = 0; index < ACPI_NUM_MUTEX; ++index)
    {
        AcpiGbl_MutexInfo[index].Mutex = nullptr;
        AcpiGbl_MutexInfo[index].ThreadId = ACPI_MUTEX_NOT_ACQUIRED;
        AcpiGbl_MutexInfo[index].UseCount = 0;
    }
}

void clear_table_array()
{
    for(UINT32 index = 0; index < kInitialTableCapacity; ++index)
    {
        g_acpica_initial_tables[index] = {};
    }
}

bool bytes_equal(const char* left, const char* right, size_t length)
{
    for(size_t index = 0; index < length; ++index)
    {
        if(left[index] != right[index])
        {
            return false;
        }
    }
    return true;
}

bool validate_checksum(const void* data, size_t length)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for(size_t index = 0; index < length; ++index)
    {
        sum = static_cast<uint8_t>(sum + bytes[index]);
    }
    return 0u == sum;
}

void reset_state_internal()
{
    if(g_acpica_state.tables_initialized || (nullptr != AcpiGbl_RootTableList.Tables))
    {
        prepare_early_mutex_state();
        AcpiTbTerminate();
    }

    clear_table_array();
    g_acpica_state = {};
    g_acpica_state.last_status = kStatusOk;
}

bool sanity_check_required_tables()
{
    char fadt_signature[] = "FACP";
    ACPI_TABLE_HEADER* fadt = nullptr;
    const ACPI_STATUS status = AcpiGetTable(fadt_signature, 1, &fadt);
    if(ACPI_FAILURE(status))
    {
        g_acpica_state.last_status = AcpiFormatException(status);
        return false;
    }

    AcpiPutTable(fadt);
    return true;
}

bool validate_rsdp(VirtualMemory& kernel_vm, const BootInfo& boot_info)
{
    if(!map_direct_range(kernel_vm, boot_info.rsdp_physical, sizeof(AcpicaRsdp)))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_NO_MEMORY);
        return false;
    }

    const auto* rsdp = kernel_physical_pointer<const AcpicaRsdp>(boot_info.rsdp_physical);
    if(!bytes_equal(rsdp->signature, "RSD PTR ", 8))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_BAD_SIGNATURE);
        debug("acpica: RSDP signature invalid")();
        return false;
    }

    if(!validate_checksum(rsdp, 20))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_BAD_CHECKSUM);
        debug("acpica: RSDP checksum invalid")();
        return false;
    }

    if(rsdp->revision > 1)
    {
        if(rsdp->length < sizeof(AcpicaRsdp))
        {
            g_acpica_state.last_status = AcpiFormatException(AE_BAD_HEADER);
            debug("acpica: RSDP length invalid")();
            return false;
        }

        if(!map_direct_range(kernel_vm, boot_info.rsdp_physical, rsdp->length))
        {
            g_acpica_state.last_status = AcpiFormatException(AE_NO_MEMORY);
            return false;
        }

        rsdp = kernel_physical_pointer<const AcpicaRsdp>(boot_info.rsdp_physical);
        if(!validate_checksum(rsdp, rsdp->length))
        {
            g_acpica_state.last_status = AcpiFormatException(AE_BAD_CHECKSUM);
            debug("acpica: RSDP extended checksum invalid")();
            return false;
        }
    }

    return true;
}

bool table_signature_equals(const ACPI_TABLE_HEADER& table, const char* signature)
{
    return bytes_equal(table.Signature, signature, 4);
}

bool descriptor_signature_equals(const ACPI_TABLE_DESC& descriptor, const char* signature)
{
    return bytes_equal(descriptor.Signature.Ascii, signature, 4);
}
}  // namespace

bool acpica_initialize_tables(VirtualMemory& kernel_vm, const BootInfo& boot_info)
{
    reset_state_internal();

    g_acpica_state.kernel_vm = &kernel_vm;
    g_acpica_state.boot_info = &boot_info;
    g_acpica_state.stage = AcpicaBootStage::TableDiscovery;

    if(0 == boot_info.rsdp_physical)
    {
        g_acpica_state.last_status = AcpiFormatException(AE_NOT_FOUND);
        debug("acpica: boot did not supply RSDP")();
        g_acpica_state.stage = AcpicaBootStage::Inactive;
        return false;
    }

    if(!validate_rsdp(kernel_vm, boot_info))
    {
        g_acpica_state.stage = AcpicaBootStage::Inactive;
        return false;
    }

    prepare_early_mutex_state();

    debug("acpica: version ")(kAcpicaPinnedRelease)(" sha=")(kAcpicaPinnedShaShort)();

    const ACPI_STATUS status = AcpiInitializeTables(g_acpica_initial_tables,
                                                    kInitialTableCapacity,
                                                    TRUE);
    if(ACPI_FAILURE(status))
    {
        g_acpica_state.last_status = AcpiFormatException(status);
        debug("acpica: table init failed status=")(g_acpica_state.last_status)();
        reset_state_internal();
        return false;
    }

    g_acpica_state.tables_initialized = true;
    g_acpica_state.stage = AcpicaBootStage::TablesReady;

    if(!sanity_check_required_tables())
    {
        debug("acpica: required table lookup failed status=")(g_acpica_state.last_status)();
        reset_state_internal();
        return false;
    }

    debug("acpica: tables ready")();
    return true;
}

bool acpica_discover_tables(uint64_t& madt_physical,
                            uint64_t& mcfg_physical,
                            uint64_t& hpet_physical,
                            uint64_t& fadt_physical,
                            uint64_t* ssdt_physical,
                            size_t ssdt_capacity,
                            size_t& ssdt_count)
{
    madt_physical = 0;
    mcfg_physical = 0;
    hpet_physical = 0;
    fadt_physical = 0;
    ssdt_count = 0;

    if((0u != ssdt_capacity) && (nullptr == ssdt_physical))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_BAD_PARAMETER);
        return false;
    }

    if(!g_acpica_state.tables_initialized)
    {
        g_acpica_state.last_status = AcpiFormatException(AE_NOT_FOUND);
        debug("acpica: discover tables before init")();
        return false;
    }

    for(UINT32 index = 0; index < AcpiGbl_RootTableList.CurrentTableCount; ++index)
    {
        const ACPI_TABLE_DESC& descriptor = AcpiGbl_RootTableList.Tables[index];
        if(0 == descriptor.Address)
        {
            continue;
        }
        if(descriptor_signature_equals(descriptor, "APIC"))
        {
            madt_physical = descriptor.Address;
        }
        else if(descriptor_signature_equals(descriptor, "MCFG"))
        {
            mcfg_physical = descriptor.Address;
        }
        else if(descriptor_signature_equals(descriptor, "HPET"))
        {
            hpet_physical = descriptor.Address;
        }
        else if(descriptor_signature_equals(descriptor, "FACP"))
        {
            fadt_physical = descriptor.Address;
        }
        else if(descriptor_signature_equals(descriptor, "SSDT"))
        {
            if(ssdt_count >= ssdt_capacity)
            {
                g_acpica_state.last_status = AcpiFormatException(AE_LIMIT);
                debug("acpica: too many SSDTs")();
                return false;
            }
            ssdt_physical[ssdt_count++] = descriptor.Address;
        }
    }

    if((0 == madt_physical) || (0 == mcfg_physical) || (0 == fadt_physical))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_NOT_FOUND);
        debug("acpica: required tables missing madt=")(0 != madt_physical)(" mcfg=")(
            0 != mcfg_physical)(" fadt=")(0 != fadt_physical)();
        return false;
    }

    g_acpica_state.last_status = kStatusOk;
    return true;
}

bool acpica_tables_initialized()
{
    return g_acpica_state.tables_initialized;
}

const char* acpica_last_status()
{
    return g_acpica_state.last_status;
}

void acpica_reset_for_tests()
{
    reset_state_internal();
}

VirtualMemory* acpica_kernel_vm()
{
    return g_acpica_state.kernel_vm;
}

const BootInfo* acpica_boot_info()
{
    return g_acpica_state.boot_info;
}

bool acpica_context_active()
{
    return nullptr != g_acpica_state.boot_info;
}

const char* acpica_boot_stage_name()
{
    switch(g_acpica_state.stage)
    {
        case AcpicaBootStage::TableDiscovery:
            return "table-discovery";
        case AcpicaBootStage::TablesReady:
            return "tables-ready";
        default:
            return "inactive";
    }
}

extern "C" ACPI_STATUS AcpiNsLoadTable(UINT32, ACPI_NAMESPACE_NODE*)
{
    return AE_SUPPORT;
}

extern "C" void AcpiNsDeleteNamespaceByOwner(ACPI_OWNER_ID)
{
}