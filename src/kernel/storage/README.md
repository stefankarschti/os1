# Kernel Storage Layer

This directory owns generic storage abstractions above concrete device drivers. The current live file is `block_device.hpp`, a small facade used by `virtio-blk` and intended to grow into request ownership, caching, and filesystem-facing block access.

Concrete hardware drivers remain under `drivers/block/`.
