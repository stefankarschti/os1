#pragma once

struct PciDriver;

const PciDriver& virtio_net_pci_driver();
bool run_virtio_net_smoke();