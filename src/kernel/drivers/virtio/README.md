# Shared Virtio Transport

This directory is reserved for virtio transport helpers that are shared by at least two virtio devices.

Keep feature negotiation, PCI capability discovery, virtqueue setup, and notification helpers here once `virtio-blk` and a second virtio driver need common code.
