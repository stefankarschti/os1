# Kernel Bus Drivers

This directory owns the small static bus/device model used after platform
discovery. ACPI and PCI enumeration still live under `platform/`; this layer
binds drivers to the enumerated records and owns driver-visible resources.

Live pieces:

- `driver_registry.*`: fixed-size PCI driver registry.
- `pci_bus.*`: walks enumerated PCI functions, probes registered drivers, and
  exposes a remove hook.
- `device.*`: publishes bound device records and lifecycle state.
- `resource.*`: claims and releases PCI BAR ownership by `DeviceId`.

Current limits:

- Matching is minimal and static. There is no dynamic module loading.
- Hot-remove is a resource-release skeleton, not a hardware hotplug event path.
- Observe output does not yet expose bindings or resource tables.
