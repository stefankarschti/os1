# Kernel Network Drivers

This directory is reserved for NIC drivers. The first likely implementation is `virtio-net` after the block path has a shared virtio transport and the networking milestone begins.

Network protocol stacks and socket/syscall code should not live here; this folder owns hardware-facing NIC logic only.
