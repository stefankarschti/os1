// HPET MMIO probing helpers used by platform discovery and later timer work.
#pragma once

// Probe the mapped HPET MMIO block and publish capability fields into
// platform state. Returns true even when HPET is absent so discovery can keep
// falling back to PIT-based timing.
bool platform_hpet_initialize();