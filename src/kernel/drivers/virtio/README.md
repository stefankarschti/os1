# Shared Virtio Transport

This directory contains virtio code shared by concrete virtio drivers.
`virtio-blk` is the first user.

Live pieces:

- `pci_transport.*`: modern virtio PCI capability discovery, BAR claiming,
  common/device/notify/ISR mapping, feature negotiation, queue setup,
  notification, and queue interrupt binding.
- `virtqueue.*`: fixed-size virtqueue allocation, descriptor/avail/used ring
  layout, submit, used-ring draining, and release.

Current limits:

- Queue size is capped at 8 and current block use keeps one request in flight.
- MSI-X/MSI/INTx selection is delegated to the PCI interrupt layer.
- The transport is intentionally protocol-agnostic; block-specific request
  headers and status handling stay in `drivers/block/virtio_blk.cpp`.
