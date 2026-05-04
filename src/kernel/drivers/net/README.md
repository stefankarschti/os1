# Kernel Network Drivers

This directory owns hardware-facing NIC drivers. `virtio-net` is the first live implementation and reuses the shared virtio PCI transport and virtqueue helpers.

Network protocol stacks and socket/syscall code should not live here; this folder owns hardware-facing NIC logic only.
