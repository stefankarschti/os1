# Kernel Storage Layer

This directory owns generic storage abstractions above concrete device drivers.
Concrete hardware drivers remain under `drivers/block/`.

`block_device.hpp` now exposes a request-shaped block facade:

- `BlockOperation`: read, write, and flush operations.
- `BlockRequestStatus`: pending, busy, success, device error, invalid, and
  timeout.
- `BlockRequest`: sector, sector count, buffer, completion flag, status, and
  bytes transferred.
- `BlockDevice`: geometry, queue depth, max sectors per request, driver state,
  submit callback, and flush callback.
- `block_request_complete()` and `block_request_wait()` as the generic
  completion and synchronous wait helpers.
- `block_read_sync()` and `block_write_sync()` wrappers for early callers and
  smoke tests.

Current limits:

- There is no block scheduler, cache, filesystem-facing buffer cache, or request
  queue yet.
- Synchronous wrappers submit one sector at a time and retry `Busy`
  submissions.
- After the scheduler is online, synchronous waiters can sleep with
  `ThreadWaitReason::BlockIo`; pre-scheduler callers still rely on the driver to
  complete or poll locally.
- Flush is represented in the ABI but `virtio-blk` does not implement a flush
  feature path yet.
