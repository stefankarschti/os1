#pragma once

#include <stddef.h>
#include <stdint.h>

#include "handoff/boot_info.hpp"
#include "platform/types.hpp"

class VirtualMemory;
enum class AcpiPowerState : uint8_t;
struct AcpiDeviceInfo;
struct AcpiPciRoute;

bool acpica_initialize_tables(VirtualMemory& kernel_vm, const BootInfo& boot_info);
bool acpica_discover_tables(uint64_t& madt_physical,
							uint64_t& mcfg_physical,
							uint64_t& hpet_physical,
							uint64_t& fadt_physical,
							uint64_t* ssdt_physical,
							size_t ssdt_capacity,
							size_t& ssdt_count);
bool acpica_parse_madt(uint64_t& lapic_base,
					   CpuInfo* cpus,
					   size_t& cpu_count,
					   IoApicInfo* ioapics,
					   size_t& ioapic_count,
					   InterruptOverride* overrides,
					   size_t& override_count);
bool acpica_parse_mcfg(PciEcamRegion* ecam_regions, size_t& ecam_region_count);
bool acpica_parse_hpet(HpetInfo& hpet);
bool acpica_load_namespace();
const char* acpica_namespace_last_error();
const char* acpica_namespace_last_object();
bool acpica_build_device_info(AcpiDeviceInfo* devices,
							  size_t& device_count,
							  AcpiPciRoute* routes,
							  size_t& route_count);
bool acpica_resolve_pci_route_details(uint8_t bus,
								  uint8_t slot,
								  uint8_t function,
								  uint8_t pin,
								  uint32_t& irq,
								  uint16_t& flags,
								  bool& source_is_gsi);
bool acpica_set_device_power_state(const char* path, AcpiPowerState state);
bool acpica_read_named_integer(const char* path, uint64_t& value);
bool acpica_tables_initialized();
const char* acpica_last_status();
void acpica_reset_for_tests();