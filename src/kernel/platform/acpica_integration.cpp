#include "platform/acpica_integration.hpp"

#include <stddef.h>
#include <stdint.h>

extern "C"
{
#include "acpi.h"
#include "actbl1.h"
#include "actbl2.h"
#include "aclocal.h"
#include "acobject.h"
#include "actables.h"
#include "acglobal.h"
}

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "platform/acpica_internal.hpp"

#if !defined(OS1_HOST_TEST)
#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/x86.hpp"
#endif

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

bool descriptor_signature_equals(const ACPI_TABLE_DESC& descriptor, const char* signature)
{
    return bytes_equal(descriptor.Signature.Ascii, signature, 4);
}

[[nodiscard]] uint8_t current_apic_id()
{
#if defined(OS1_HOST_TEST)
    return 0;
#else
    cpuinfo info{};
    cpuid(1, &info);
    return static_cast<uint8_t>((info.ebx >> 24) & 0xFFu);
#endif
}

bool set_status_error(ACPI_STATUS status, const char* message)
{
    g_acpica_state.last_status = AcpiFormatException(status);
    debug(message)(" status=")(g_acpica_state.last_status)();
    return false;
}

bool ensure_tables_ready(const char* operation)
{
    if(g_acpica_state.tables_initialized)
    {
        return true;
    }

    g_acpica_state.last_status = AcpiFormatException(AE_NOT_FOUND);
    debug("acpica: ")(operation)(" before init")();
    return false;
}

template<typename TableType>
bool get_table(const char* signature, TableType*& table)
{
    table = nullptr;
    ACPI_TABLE_HEADER* raw = nullptr;
    const ACPI_STATUS status = AcpiGetTable(const_cast<char*>(signature), 1, &raw);
    if(ACPI_FAILURE(status))
    {
        g_acpica_state.last_status = AcpiFormatException(status);
        debug("acpica: get table failed sig=")(signature)(" status=")(
            g_acpica_state.last_status)();
        return false;
    }

    table = reinterpret_cast<TableType*>(raw);
    return true;
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

bool acpica_parse_madt(uint64_t& lapic_base,
                       CpuInfo* cpus,
                       size_t& cpu_count,
                       IoApicInfo* ioapics,
                       size_t& ioapic_count,
                       InterruptOverride* overrides,
                       size_t& override_count)
{
    if((nullptr == cpus) || (nullptr == ioapics) || (nullptr == overrides))
    {
        g_acpica_state.last_status = AcpiFormatException(AE_BAD_PARAMETER);
        return false;
    }
    if(!ensure_tables_ready("parse MADT"))
    {
        return false;
    }

    ACPI_TABLE_MADT* madt = nullptr;
    if(!get_table(ACPI_SIG_MADT, madt))
    {
        return false;
    }

    if(madt->Header.Length < sizeof(ACPI_TABLE_MADT))
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
        return set_status_error(AE_BAD_HEADER, "acpica: MADT too short");
    }

    lapic_base = madt->Address;
    cpu_count = 0;
    ioapic_count = 0;
    override_count = 0;

    const auto* cursor = reinterpret_cast<const uint8_t*>(madt) + sizeof(ACPI_TABLE_MADT);
    const auto* end = reinterpret_cast<const uint8_t*>(madt) + madt->Header.Length;
    while(cursor < end)
    {
        if((cursor + sizeof(ACPI_SUBTABLE_HEADER)) > end)
        {
            AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
            return set_status_error(AE_BAD_HEADER, "acpica: MADT truncated");
        }

        const auto* subtable = reinterpret_cast<const ACPI_SUBTABLE_HEADER*>(cursor);
        if((subtable->Length < sizeof(ACPI_SUBTABLE_HEADER)) || ((cursor + subtable->Length) > end))
        {
            AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
            return set_status_error(AE_BAD_HEADER, "acpica: MADT entry length invalid");
        }

        switch(subtable->Type)
        {
            case ACPI_MADT_TYPE_LOCAL_APIC: {
                const auto* entry = reinterpret_cast<const ACPI_MADT_LOCAL_APIC*>(subtable);
                if(entry->LapicFlags & ACPI_MADT_ENABLED)
                {
                    if(cpu_count >= kPlatformMaxCpus)
                    {
                        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
                        g_acpica_state.last_status = AcpiFormatException(AE_LIMIT);
                        debug("acpica: CPU table full")();
                        return false;
                    }
                    CpuInfo& cpu = cpus[cpu_count++];
                    cpu.apic_id = entry->Id;
                    cpu.enabled = true;
                    cpu.is_bsp = false;
                }
                break;
            }
            case ACPI_MADT_TYPE_IO_APIC: {
                const auto* entry = reinterpret_cast<const ACPI_MADT_IO_APIC*>(subtable);
                if(ioapic_count >= kPlatformMaxIoApics)
                {
                    AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
                    g_acpica_state.last_status = AcpiFormatException(AE_LIMIT);
                    debug("acpica: IOAPIC table full")();
                    return false;
                }
                IoApicInfo& ioapic = ioapics[ioapic_count++];
                ioapic.id = entry->Id;
                ioapic.address = entry->Address;
                ioapic.gsi_base = entry->GlobalIrqBase;
                break;
            }
            case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: {
                const auto* entry = reinterpret_cast<const ACPI_MADT_INTERRUPT_OVERRIDE*>(subtable);
                if(0u == entry->Bus)
                {
                    if(override_count >= kPlatformMaxInterruptOverrides)
                    {
                        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));
                        g_acpica_state.last_status = AcpiFormatException(AE_LIMIT);
                        debug("acpica: interrupt override table full")();
                        return false;
                    }
                    InterruptOverride& irq_override = overrides[override_count++];
                    irq_override.bus_irq = entry->SourceIrq;
                    irq_override.flags = entry->IntiFlags;
                    irq_override.global_irq = entry->GlobalIrq;
                }
                break;
            }
            case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE: {
                const auto* entry = reinterpret_cast<const ACPI_MADT_LOCAL_APIC_OVERRIDE*>(subtable);
                lapic_base = entry->Address;
                break;
            }
            default:
                break;
        }

        cursor += subtable->Length;
    }

    AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(madt));

    if((0 == cpu_count) || (0 == ioapic_count) || (0 == lapic_base))
    {
        return set_status_error(AE_NOT_FOUND, "acpica: MADT missing required topology");
    }

    const uint8_t bsp_apic_id = current_apic_id();
    bool found_bsp = false;
    for(size_t index = 0; index < cpu_count; ++index)
    {
        if(cpus[index].apic_id == bsp_apic_id)
        {
            cpus[index].is_bsp = true;
            found_bsp = true;
            break;
        }
    }
    if(!found_bsp)
    {
        return set_status_error(AE_NOT_FOUND, "acpica: BSP APIC ID not found in MADT");
    }

    g_acpica_state.last_status = kStatusOk;
    debug("acpi: MADT ready cpus=")(cpu_count)(" ioapics=")(ioapic_count)(" overrides=")(
        override_count)();
    return true;
}

bool acpica_parse_mcfg(PciEcamRegion* ecam_regions, size_t& ecam_region_count)
{
    if(nullptr == ecam_regions)
    {
        g_acpica_state.last_status = AcpiFormatException(AE_BAD_PARAMETER);
        return false;
    }
    if(!ensure_tables_ready("parse MCFG"))
    {
        return false;
    }

    ACPI_TABLE_MCFG* mcfg = nullptr;
    if(!get_table(ACPI_SIG_MCFG, mcfg))
    {
        return false;
    }

    if(mcfg->Header.Length < sizeof(ACPI_TABLE_MCFG))
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
        return set_status_error(AE_BAD_HEADER, "acpica: MCFG too short");
    }

    ecam_region_count = 0;
    const uint32_t payload_length = mcfg->Header.Length - sizeof(ACPI_TABLE_MCFG);
    if(0u != (payload_length % sizeof(ACPI_MCFG_ALLOCATION)))
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
        return set_status_error(AE_BAD_HEADER, "acpica: MCFG length misaligned");
    }

    const auto* entries = reinterpret_cast<const ACPI_MCFG_ALLOCATION*>(
        reinterpret_cast<const uint8_t*>(mcfg) + sizeof(ACPI_TABLE_MCFG));
    const size_t entry_count = payload_length / sizeof(ACPI_MCFG_ALLOCATION);
    if(0u == entry_count)
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
        return set_status_error(AE_NOT_FOUND, "acpica: MCFG contains no ECAM regions");
    }

    for(size_t index = 0; index < entry_count; ++index)
    {
        if(ecam_region_count >= kPlatformMaxPciEcamRegions)
        {
            AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
            g_acpica_state.last_status = AcpiFormatException(AE_LIMIT);
            debug("acpica: ECAM region table full")();
            return false;
        }
        if(entries[index].StartBusNumber > entries[index].EndBusNumber)
        {
            AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
            return set_status_error(AE_BAD_VALUE, "acpica: invalid ECAM bus range");
        }

        PciEcamRegion& region = ecam_regions[ecam_region_count++];
        region.base_address = entries[index].Address;
        region.segment_group = entries[index].PciSegment;
        region.bus_start = entries[index].StartBusNumber;
        region.bus_end = entries[index].EndBusNumber;
    }

    AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(mcfg));
    g_acpica_state.last_status = kStatusOk;
    debug("acpi: MCFG ready regions=")(ecam_region_count)();
    return true;
}

bool acpica_parse_hpet(HpetInfo& hpet)
{
    if(!ensure_tables_ready("parse HPET"))
    {
        return false;
    }

    ACPI_TABLE_HPET* hpet_table = nullptr;
    if(!get_table(ACPI_SIG_HPET, hpet_table))
    {
        return false;
    }

    if(hpet_table->Header.Length < sizeof(ACPI_TABLE_HPET))
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(hpet_table));
        return set_status_error(AE_BAD_HEADER, "acpica: HPET too short");
    }

    if((hpet_table->Address.SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY) ||
       (0u == hpet_table->Address.Address))
    {
        AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(hpet_table));
        return set_status_error(AE_BAD_ADDRESS, "acpica: HPET has unsupported base address");
    }

    hpet = {};
    hpet.present = true;
    hpet.hpet_number = hpet_table->Sequence;
    hpet.page_protection = static_cast<uint8_t>(hpet_table->Flags & ACPI_HPET_PAGE_PROTECT_MASK);
    hpet.minimum_tick = hpet_table->MinimumTick;
    hpet.physical_address = hpet_table->Address.Address;

    AcpiPutTable(reinterpret_cast<ACPI_TABLE_HEADER*>(hpet_table));
    g_acpica_state.last_status = kStatusOk;
    debug("acpi: HPET discovered base=0x")(hpet.physical_address, 16)(" number=")(
        hpet.hpet_number)(" min_tick=")(hpet.minimum_tick)();
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