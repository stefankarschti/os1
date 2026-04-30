// ACPI table parser for platform discovery. It normalizes RSDP/XSDT/RSDT,
// MADT, and MCFG content into platform-state records consumed by topology,
// interrupt routing, and PCI enumeration.
#include "platform/acpi.hpp"

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "mm/boot_mapping.hpp"
#include "mm/virtual_memory.hpp"

#if defined(OS1_HOST_TEST)
#include <string.h>
#else
#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/x86.hpp"
#include "util/string.h"
#endif

namespace
{
constexpr uint64_t kAcpiMaxTableLength = 1ull << 20;
constexpr uint8_t kAcpiIsaBus = 0;
constexpr uint8_t kAcpiMadtTypeLocalApic = 0;
constexpr uint8_t kAcpiMadtTypeIoApic = 1;
constexpr uint8_t kAcpiMadtTypeInterruptOverride = 2;
constexpr uint8_t kAcpiMadtTypeLocalApicAddressOverride = 5;
constexpr uint8_t kAcpiAddressSpaceSystemMemory = 0;

struct [[gnu::packed]] AcpiGas
{
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
};

struct [[gnu::packed]] AcpiRsdp
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

struct [[gnu::packed]] AcpiSdtHeader
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct [[gnu::packed]] AcpiMadt
{
    AcpiSdtHeader header;
    uint32_t lapic_address;
    uint32_t flags;
};

struct [[gnu::packed]] AcpiMadtLocalApic
{
    uint8_t type;
    uint8_t length;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
};

struct [[gnu::packed]] AcpiMadtIoApic
{
    uint8_t type;
    uint8_t length;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
};

struct [[gnu::packed]] AcpiMadtInterruptOverride
{
    uint8_t type;
    uint8_t length;
    uint8_t bus;
    uint8_t source_irq;
    uint32_t global_irq;
    uint16_t flags;
};

struct [[gnu::packed]] AcpiMadtLocalApicAddressOverride
{
    uint8_t type;
    uint8_t length;
    uint16_t reserved;
    uint64_t lapic_address;
};

struct [[gnu::packed]] AcpiMcfg
{
    AcpiSdtHeader header;
    uint64_t reserved;
};

struct [[gnu::packed]] AcpiMcfgEntry
{
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t bus_start;
    uint8_t bus_end;
    uint32_t reserved;
};

struct [[gnu::packed]] AcpiHpet
{
    AcpiSdtHeader header;
    uint32_t event_timer_block_id;
    AcpiGas base_address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
};

struct [[gnu::packed]] AcpiFadt
{
    AcpiSdtHeader header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved0;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_request;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_block_length;
    uint8_t gpe1_block_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t c2_latency;
    uint16_t c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_architecture_flags;
    uint8_t reserved1;
    uint32_t flags;
    AcpiGas reset_register;
    uint8_t reset_value;
    uint16_t arm_boot_architecture_flags;
    uint8_t minor_version;
    uint64_t x_firmware_control;
    uint64_t x_dsdt;
};

struct AcpiRootTables
{
    uint64_t madt_physical;
    uint64_t mcfg_physical;
    uint64_t hpet_physical;
    uint64_t fadt_physical;
    uint64_t ssdt_physical[kPlatformMaxAcpiDefinitionBlocks];
    size_t ssdt_count;
};

struct AcpiOutput
{
    uint64_t& lapic_base;
    CpuInfo* cpus;
    size_t& cpu_count;
    IoApicInfo* ioapics;
    size_t& ioapic_count;
    InterruptOverride* overrides;
    size_t& override_count;
    PciEcamRegion* ecam_regions;
    size_t& ecam_region_count;
    HpetInfo& hpet;
    AcpiFixedInfo& acpi_fixed;
    AcpiDefinitionBlock* definition_blocks;
    size_t& definition_block_count;
};

[[nodiscard]] inline uint64_t align_down(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

[[nodiscard]] inline uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] bool validate_checksum(const void* base, size_t length)
{
    const auto* bytes = static_cast<const uint8_t*>(base);
    uint8_t sum = 0;
    for(size_t i = 0; i < length; ++i)
    {
        sum = static_cast<uint8_t>(sum + bytes[i]);
    }
    return 0 == sum;
}

[[nodiscard]] bool signature_equals(const char* left, const char* right, size_t length)
{
    return 0 == memcmp(left, right, length);
}

void copy_signature(char (&destination)[4], const char* source)
{
    memcpy(destination, source, sizeof(destination));
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

[[nodiscard]] bool map_acpi_range(VirtualMemory& kernel_vm,
                                  uint64_t physical_address,
                                  uint64_t length)
{
    if((0 == physical_address) || (0 == length))
    {
        return false;
    }
    return map_direct_range(kernel_vm, physical_address, length);
}

template<typename T>
[[nodiscard]] const T* map_acpi_object(VirtualMemory& kernel_vm, uint64_t physical_address)
{
    if(!map_acpi_range(kernel_vm, physical_address, sizeof(T)))
    {
        return nullptr;
    }
    return kernel_physical_pointer<const T>(physical_address);
}

[[nodiscard]] const AcpiSdtHeader* map_acpi_table(VirtualMemory& kernel_vm,
                                                  uint64_t physical_address,
                                                  const char* expected_signature)
{
    const AcpiSdtHeader* header = map_acpi_object<AcpiSdtHeader>(kernel_vm, physical_address);
    if(nullptr == header)
    {
        return nullptr;
    }
    if(expected_signature && !signature_equals(header->signature, expected_signature, 4))
    {
        debug("acpi: unexpected signature at 0x")(physical_address, 16)();
        return nullptr;
    }
    if((header->length < sizeof(AcpiSdtHeader)) || (header->length > kAcpiMaxTableLength))
    {
        debug("acpi: invalid table length 0x")(header->length, 16)(" at 0x")(physical_address,
                                                                             16)();
        return nullptr;
    }
    if(!map_acpi_range(kernel_vm, physical_address, header->length))
    {
        return nullptr;
    }
    header = kernel_physical_pointer<const AcpiSdtHeader>(physical_address);
    if(!validate_checksum(header, header->length))
    {
        debug("acpi: checksum failed at 0x")(physical_address, 16)();
        return nullptr;
    }
    return header;
}

[[nodiscard]] bool add_interrupt_override(AcpiOutput& output,
                                          uint8_t bus_irq,
                                          uint32_t global_irq,
                                          uint16_t flags)
{
    if(output.override_count >= kPlatformMaxInterruptOverrides)
    {
        debug("acpi: interrupt override table full")();
        return false;
    }
    InterruptOverride& entry = output.overrides[output.override_count++];
    entry.bus_irq = bus_irq;
    entry.flags = flags;
    entry.global_irq = global_irq;
    return true;
}

[[nodiscard]] bool parse_madt(VirtualMemory& kernel_vm,
                              uint64_t physical_address,
                              AcpiOutput& output)
{
    const auto* header = map_acpi_table(kernel_vm, physical_address, "APIC");
    if(nullptr == header)
    {
        return false;
    }

    const auto* madt = reinterpret_cast<const AcpiMadt*>(header);
    output.lapic_base = madt->lapic_address;
    output.cpu_count = 0;
    output.ioapic_count = 0;
    output.override_count = 0;

    const auto* cursor = reinterpret_cast<const uint8_t*>(madt + 1);
    const auto* end = reinterpret_cast<const uint8_t*>(madt) + madt->header.length;
    while(cursor < end)
    {
        if((cursor + 2) > end)
        {
            debug("acpi: MADT truncated")();
            return false;
        }
        const uint8_t type = cursor[0];
        const uint8_t length = cursor[1];
        if((length < 2) || ((cursor + length) > end))
        {
            debug("acpi: MADT entry length invalid")();
            return false;
        }

        switch(type)
        {
            case kAcpiMadtTypeLocalApic: {
                const auto* entry = reinterpret_cast<const AcpiMadtLocalApic*>(cursor);
                if(entry->flags & 0x1u)
                {
                    if(output.cpu_count >= kPlatformMaxCpus)
                    {
                        debug("acpi: CPU table full")();
                        return false;
                    }
                    CpuInfo& cpu = output.cpus[output.cpu_count++];
                    cpu.apic_id = entry->apic_id;
                    cpu.enabled = true;
                    cpu.is_bsp = false;
                }
            }
            break;
            case kAcpiMadtTypeIoApic: {
                const auto* entry = reinterpret_cast<const AcpiMadtIoApic*>(cursor);
                if(output.ioapic_count >= kPlatformMaxIoApics)
                {
                    debug("acpi: IOAPIC table full")();
                    return false;
                }
                IoApicInfo& ioapic_info = output.ioapics[output.ioapic_count++];
                ioapic_info.id = entry->ioapic_id;
                ioapic_info.address = entry->ioapic_address;
                ioapic_info.gsi_base = entry->gsi_base;
            }
            break;
            case kAcpiMadtTypeInterruptOverride: {
                const auto* entry = reinterpret_cast<const AcpiMadtInterruptOverride*>(cursor);
                if(entry->bus == kAcpiIsaBus)
                {
                    if(!add_interrupt_override(
                           output, entry->source_irq, entry->global_irq, entry->flags))
                    {
                        return false;
                    }
                }
            }
            break;
            case kAcpiMadtTypeLocalApicAddressOverride: {
                const auto* entry =
                    reinterpret_cast<const AcpiMadtLocalApicAddressOverride*>(cursor);
                output.lapic_base = entry->lapic_address;
            }
            break;
            default:
                break;
        }

        cursor += length;
    }

    if((0 == output.cpu_count) || (0 == output.ioapic_count) || (0 == output.lapic_base))
    {
        debug("acpi: MADT missing required topology")();
        return false;
    }

    const uint8_t bsp_apic_id = current_apic_id();
    bool found_bsp = false;
    for(size_t i = 0; i < output.cpu_count; ++i)
    {
        if(output.cpus[i].apic_id == bsp_apic_id)
        {
            output.cpus[i].is_bsp = true;
            found_bsp = true;
            break;
        }
    }
    if(!found_bsp)
    {
        debug("acpi: BSP APIC ID not found in MADT")();
        return false;
    }

    debug("acpi: MADT ready cpus=")(output.cpu_count)(" ioapics=")(output.ioapic_count)(
        " overrides=")(output.override_count)();
    return true;
}

[[nodiscard]] bool parse_mcfg(VirtualMemory& kernel_vm,
                              uint64_t physical_address,
                              AcpiOutput& output)
{
    const auto* header = map_acpi_table(kernel_vm, physical_address, "MCFG");
    if(nullptr == header)
    {
        return false;
    }
    if(header->length < sizeof(AcpiMcfg))
    {
        debug("acpi: MCFG too short")();
        return false;
    }

    output.ecam_region_count = 0;
    const uint32_t payload_length = header->length - sizeof(AcpiMcfg);
    if(0 != (payload_length % sizeof(AcpiMcfgEntry)))
    {
        debug("acpi: MCFG length misaligned")();
        return false;
    }

    const auto* entries = reinterpret_cast<const AcpiMcfgEntry*>(
        reinterpret_cast<const uint8_t*>(header) + sizeof(AcpiMcfg));
    const size_t entry_count = payload_length / sizeof(AcpiMcfgEntry);
    if(0 == entry_count)
    {
        debug("acpi: MCFG contains no ECAM regions")();
        return false;
    }

    for(size_t i = 0; i < entry_count; ++i)
    {
        if(output.ecam_region_count >= kPlatformMaxPciEcamRegions)
        {
            debug("acpi: ECAM region table full")();
            return false;
        }
        if(entries[i].bus_start > entries[i].bus_end)
        {
            debug("acpi: invalid ECAM bus range")();
            return false;
        }

        PciEcamRegion& region = output.ecam_regions[output.ecam_region_count++];
        region.base_address = entries[i].base_address;
        region.segment_group = entries[i].segment_group;
        region.bus_start = entries[i].bus_start;
        region.bus_end = entries[i].bus_end;
    }

    debug("acpi: MCFG ready regions=")(output.ecam_region_count)();
    return true;
}

[[nodiscard]] bool parse_hpet(VirtualMemory& kernel_vm,
                              uint64_t physical_address,
                              AcpiOutput& output)
{
    const auto* header = map_acpi_table(kernel_vm, physical_address, "HPET");
    if(nullptr == header)
    {
        return false;
    }
    if(header->length < sizeof(AcpiHpet))
    {
        debug("acpi: HPET too short")();
        return false;
    }

    const auto* hpet = reinterpret_cast<const AcpiHpet*>(header);
    if((hpet->base_address.address_space_id != kAcpiAddressSpaceSystemMemory) ||
       (0 == hpet->base_address.address))
    {
        debug("acpi: HPET has unsupported base address")();
        return false;
    }

    output.hpet = {};
    output.hpet.present = true;
    output.hpet.hpet_number = hpet->hpet_number;
    output.hpet.page_protection = hpet->page_protection;
    output.hpet.minimum_tick = hpet->minimum_tick;
    output.hpet.physical_address = hpet->base_address.address;
    debug("acpi: HPET discovered base=0x")(output.hpet.physical_address, 16)(" number=")(
        output.hpet.hpet_number)(" min_tick=")(output.hpet.minimum_tick)();
    return true;
}

[[nodiscard]] bool add_definition_block(AcpiOutput& output,
                                        uint64_t physical_address,
                                        const AcpiSdtHeader& header)
{
    if(nullptr == output.definition_blocks)
    {
        return false;
    }
    if(output.definition_block_count >= kPlatformMaxAcpiDefinitionBlocks)
    {
        debug("acpi: definition-block table full")();
        return false;
    }

    AcpiDefinitionBlock& block = output.definition_blocks[output.definition_block_count++];
    block = {};
    block.active = true;
    copy_signature(block.signature, header.signature);
    block.length = header.length;
    block.physical_address = physical_address;
    return true;
}

[[nodiscard]] bool parse_fadt(VirtualMemory& kernel_vm,
                              uint64_t physical_address,
                              AcpiOutput& output)
{
    const auto* header = map_acpi_table(kernel_vm, physical_address, "FACP");
    if(nullptr == header)
    {
        return false;
    }
    if(header->length < offsetof(AcpiFadt, flags) + sizeof(uint32_t))
    {
        debug("acpi: FADT too short")();
        return false;
    }

    const auto* fadt = reinterpret_cast<const AcpiFadt*>(header);
    uint64_t firmware_ctrl = fadt->firmware_ctrl;
    if((header->length >= offsetof(AcpiFadt, x_firmware_control) + sizeof(uint64_t)) &&
       (0 != fadt->x_firmware_control))
    {
        firmware_ctrl = fadt->x_firmware_control;
    }

    uint64_t dsdt_physical = fadt->dsdt;
    if((header->length >= offsetof(AcpiFadt, x_dsdt) + sizeof(uint64_t)) && (0 != fadt->x_dsdt))
    {
        dsdt_physical = fadt->x_dsdt;
    }
    if(0 == dsdt_physical)
    {
        debug("acpi: FADT did not provide a DSDT")();
        return false;
    }

    const auto* dsdt = map_acpi_table(kernel_vm, dsdt_physical, "DSDT");
    if(nullptr == dsdt)
    {
        return false;
    }

    output.acpi_fixed = {};
    output.acpi_fixed.present = true;
    output.acpi_fixed.preferred_pm_profile = fadt->preferred_pm_profile;
    output.acpi_fixed.sci_interrupt = fadt->sci_interrupt;
    output.acpi_fixed.boot_architecture_flags = fadt->boot_architecture_flags;
    output.acpi_fixed.flags = fadt->flags;
    output.acpi_fixed.firmware_ctrl = firmware_ctrl;
    output.acpi_fixed.dsdt_physical = dsdt_physical;
    if(!add_definition_block(output, dsdt_physical, *dsdt))
    {
        return false;
    }

    debug("acpi: FADT ready dsdt=0x")(output.acpi_fixed.dsdt_physical, 16)(" sci=")(
        output.acpi_fixed.sci_interrupt)(" blocks=")(output.definition_block_count)();
    return true;
}

[[nodiscard]] bool resolve_acpi_tables(VirtualMemory& kernel_vm,
                                       const BootInfo& boot_info,
                                       AcpiRootTables& tables)
{
    tables = {};
    if(0 == boot_info.rsdp_physical)
    {
        debug("acpi: boot did not supply an RSDP")();
        return false;
    }
    if(!map_acpi_range(kernel_vm, boot_info.rsdp_physical, sizeof(AcpiRsdp)))
    {
        return false;
    }

    const auto* rsdp = kernel_physical_pointer<const AcpiRsdp>(boot_info.rsdp_physical);
    if(!signature_equals(rsdp->signature, "RSD PTR ", 8))
    {
        debug("acpi: RSDP signature invalid")();
        return false;
    }
    if(!validate_checksum(rsdp, 20))
    {
        debug("acpi: RSDP checksum invalid")();
        return false;
    }
    debug("boot rsdp physical=0x")(boot_info.rsdp_physical, 16)();

    uint64_t root_table_physical = 0;
    bool use_xsdt = false;
    if((rsdp->revision >= 2) && (rsdp->length >= sizeof(AcpiRsdp)) && (0 != rsdp->xsdt_address))
    {
        if(!validate_checksum(rsdp, rsdp->length))
        {
            debug("acpi: XSDP extended checksum invalid")();
            return false;
        }
        root_table_physical = rsdp->xsdt_address;
        use_xsdt = true;
    }
    else if(0 != rsdp->rsdt_address)
    {
        root_table_physical = rsdp->rsdt_address;
    }
    else
    {
        debug("acpi: neither XSDT nor RSDT is available")();
        return false;
    }

    const AcpiSdtHeader* root =
        map_acpi_table(kernel_vm, root_table_physical, use_xsdt ? "XSDT" : "RSDT");
    if((nullptr == root) && use_xsdt && (0 != rsdp->rsdt_address))
    {
        debug("acpi: XSDT unavailable, falling back to RSDT")();
        root = map_acpi_table(kernel_vm, rsdp->rsdt_address, "RSDT");
        use_xsdt = false;
    }
    if(nullptr == root)
    {
        return false;
    }

    const size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
    if(root->length < sizeof(AcpiSdtHeader))
    {
        return false;
    }
    const uint32_t payload_length = root->length - sizeof(AcpiSdtHeader);
    if(0 != (payload_length % entry_size))
    {
        debug("acpi: root table length misaligned")();
        return false;
    }

    const size_t entry_count = payload_length / entry_size;
    const auto* cursor = reinterpret_cast<const uint8_t*>(root) + sizeof(AcpiSdtHeader);
    for(size_t i = 0; i < entry_count; ++i)
    {
        const uint64_t entry_physical = use_xsdt ? reinterpret_cast<const uint64_t*>(cursor)[i]
                                                 : reinterpret_cast<const uint32_t*>(cursor)[i];
        const auto* entry_header = map_acpi_object<AcpiSdtHeader>(kernel_vm, entry_physical);
        if(nullptr == entry_header)
        {
            return false;
        }
        if(signature_equals(entry_header->signature, "APIC", 4))
        {
            tables.madt_physical = entry_physical;
        }
        else if(signature_equals(entry_header->signature, "MCFG", 4))
        {
            tables.mcfg_physical = entry_physical;
        }
        else if(signature_equals(entry_header->signature, "HPET", 4))
        {
            tables.hpet_physical = entry_physical;
        }
        else if(signature_equals(entry_header->signature, "FACP", 4))
        {
            tables.fadt_physical = entry_physical;
        }
        else if(signature_equals(entry_header->signature, "SSDT", 4))
        {
            if(tables.ssdt_count >= kPlatformMaxAcpiDefinitionBlocks)
            {
                debug("acpi: too many SSDTs")();
                return false;
            }
            tables.ssdt_physical[tables.ssdt_count++] = entry_physical;
        }
    }

    return (0 != tables.madt_physical) && (0 != tables.mcfg_physical) &&
           (0 != tables.fadt_physical);
}
}  // namespace

bool discover_acpi_platform(VirtualMemory& kernel_vm,
                            const BootInfo& boot_info,
                            uint64_t& lapic_base,
                            CpuInfo* cpus,
                            size_t& cpu_count,
                            IoApicInfo* ioapics,
                            size_t& ioapic_count,
                            InterruptOverride* overrides,
                            size_t& override_count,
                            PciEcamRegion* ecam_regions,
                            size_t& ecam_region_count,
                            HpetInfo& hpet,
                            AcpiFixedInfo& acpi_fixed,
                            AcpiDefinitionBlock* definition_blocks,
                            size_t& definition_block_count)
{
    if((nullptr == cpus) || (nullptr == ioapics) || (nullptr == overrides) ||
       (nullptr == ecam_regions) || (nullptr == definition_blocks))
    {
        return false;
    }

    definition_block_count = 0;
    acpi_fixed = {};

    AcpiRootTables tables{};
    if(!resolve_acpi_tables(kernel_vm, boot_info, tables))
    {
        return false;
    }

    AcpiOutput output{
        .lapic_base = lapic_base,
        .cpus = cpus,
        .cpu_count = cpu_count,
        .ioapics = ioapics,
        .ioapic_count = ioapic_count,
        .overrides = overrides,
        .override_count = override_count,
        .ecam_regions = ecam_regions,
        .ecam_region_count = ecam_region_count,
        .hpet = hpet,
        .acpi_fixed = acpi_fixed,
        .definition_blocks = definition_blocks,
        .definition_block_count = definition_block_count,
    };
    output.hpet = {};
    if(!parse_madt(kernel_vm, tables.madt_physical, output) ||
       !parse_mcfg(kernel_vm, tables.mcfg_physical, output) ||
       !parse_fadt(kernel_vm, tables.fadt_physical, output))
    {
        return false;
    }

    for(size_t i = 0; i < tables.ssdt_count; ++i)
    {
        const auto* ssdt = map_acpi_table(kernel_vm, tables.ssdt_physical[i], "SSDT");
        if(nullptr == ssdt)
        {
            return false;
        }
        if(!add_definition_block(output, tables.ssdt_physical[i], *ssdt))
        {
            return false;
        }
    }

    if((0 != tables.hpet_physical) && !parse_hpet(kernel_vm, tables.hpet_physical, output))
    {
        debug("acpi: ignoring unusable HPET table")();
        output.hpet = {};
    }
    return true;
}