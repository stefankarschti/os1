# Object-Oriented VFS Structure Specification

## 1. Purpose

This document specifies the initial structure of a virtual filesystem (VFS) for an object-oriented operating system.

In this design, the VFS is not only a filesystem abstraction. It is a kernel-managed hierarchical namespace that resolves paths to object handles. Each handle grants access to a typed object interface made of properties, methods, and events.

The VFS has two roles:

1. Expose persistent storage objects such as files and directories.
2. Expose runtime OS objects such as processes, devices, services, drivers, sessions, and kernel subsystems.

The native model is object-oriented. A POSIX-like file API may later be implemented as a compatibility layer over this model.

---

## 2. Core Concepts

### 2.1 Object

An object is a kernel-known entity that can be accessed through a handle.

Examples:

- File
- Directory
- Process
- Thread
- Device
- Driver
- Service
- Memory region
- Event subscription
- Kernel subsystem

Each object has:

- Stable object identity while alive
- Object kind
- Optional path in the VFS namespace
- Interface descriptor
- Access control rules
- Lifetime and reference-counting rules

Not every object needs a global path. Temporary or private objects may be handle-only.

---

### 2.2 Handle

A handle is a process-local reference to an object.

A handle contains or implies:

- Referenced object
- Granted rights
- Owning process
- Optional per-open state
- Optional cursor or stream position
- Optional subscription state

Paths are for discovery. Handles are for authority.

Example:

```text
/os/devices/gpu0 -> resolve path -> authorize -> object handle
```

After a handle is created, operations should use the handle directly rather than repeatedly resolving the path.

---

### 2.3 Interface Descriptor

Each object exposes an interface descriptor.

The descriptor lists only three interface categories:

- Properties
- Methods
- Events

Other concepts are modeled through properties whose types come from class libraries:

- Streams are read-only properties of type `Stream<T>`. The stream object may be mutated through its own methods, such as `write`, `seek`, or `flush`, if the caller has the required rights.
- Memory regions are properties of type `Collection<MemoryStream>` for extensible region sets, or `MemoryArray` for fixed memory layouts.
- Child objects are properties of type `Collection<T>`, where `T` is the child object type, such as `Collection<Process>`, `Collection<Device>`, or `Collection<File>`.

This keeps the interface model small while still allowing complex OS objects to expose rich structure.

Early implementations may use static numeric IDs. Later implementations may support richer metadata embedded in ELF sections, service descriptors, or class-library metadata.

---

### 2.4 Class Libraries

The OS should expose one or more class libraries that define reusable object and value types used by VFS interfaces.

The most important one is the standard class library. It should define primitive and common compound types used across the OS.

Examples:

```text
Bool
Int32
UInt64
String
Uuid
Timestamp
Duration
Result<T>
Optional<T>
List<T>
Collection<T>
Dictionary<K, V>
Map<K, V>
Stream<T>
MemoryStream
MemoryArray
ObjectRef<T>
EventSubscription
```

Class libraries can be:

- Built into the kernel ABI for fundamental types.
- Provided by OS-level runtime libraries for richer structures.
- Extended by drivers, services, filesystems, and user-installed components.

The kernel does not need to fully understand every high-level class. It only needs enough type information to validate access, route calls, preserve ABI compatibility, and enforce rights. Richer interpretation can be handled by user-space libraries and language bindings.

---

## 3. Native VFS Operation Model

The native VFS API should be based on a small set of uniform object operations.

Conceptual operations:

```text
open(path, requested_rights) -> handle
close(handle)
describe(handle) -> interface descriptor
get_property(handle, property_id) -> value
call_method(handle, method_id, args) -> result
subscribe_event(handle, event_id) -> subscription_handle
unsubscribe_event(subscription_handle)
wait(handles[], timeout) -> event
```

Stream I/O, memory mapping, and child traversal are not special VFS syscall categories in the semantic model. They are method calls on objects returned through properties.

Examples:

```text
file.data.read(request)
file.data.write(request)
process.memory_regions.get(index).map(request)
directory.children.lookup("name")
```

The exact syscall ABI can be decided later. This document describes the semantic model.

---

## 4. Top-Level VFS Layout

Initial proposed namespace:

```text
/
  os/
    system/
    kernel/
    processes/
    threads/
    memory/
    devices/
    drivers/
    services/
    sessions/
    users/
    mounts/
    events/
    logs/
  dev/        optional POSIX-style compatibility projection for device nodes
  home/
  bin/
  lib/
  tmp/
  proc/       optional POSIX-style compatibility projection for processes
  sys/        optional POSIX-style compatibility projection for system/device metadata
```

The `/os` branch is the native object namespace.

The `/home`, `/bin`, `/lib`, and `/tmp` branches are storage-backed namespace branches.

The `/dev`, `/proc`, and `/sys` branches are optional POSIX-style compatibility projections. They should not be the primary native interface.

Recommended interpretation:

| Branch        | Role                                                                                                                                                       |
| ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/os/devices` | Native object-oriented device namespace. Devices expose properties, methods, and events.                                                                   |
| `/dev`        | POSIX-style compatibility projection. Exposes device-node-like views backed by native device objects.                                                      |
| `/proc`       | POSIX-style compatibility projection for process information. Backed by `/os/processes`.                                                                   |
| `/sys`        | POSIX-style compatibility projection for system, bus, driver, and device metadata. Backed by `/os/system`, `/os/kernel`, `/os/devices`, and `/os/drivers`. |

For example, `/dev/net0`, `/dev/fb0`, `/dev/tty0`, or `/dev/input0` may exist as compatibility views, but the canonical native objects should be `/os/devices/net[0]`, `/os/devices/gpu[0]`, `/os/devices/keyboard[0]`, and so on.

---

## 5. Branch Specifications

## 5.1 `/os/system`

Represents the operating system as a high-level object.

Example path:

```text
/os/system
```

### Properties

| Property     | Type      | Access    | Description                                              |
| ------------ | --------- | --------- | -------------------------------------------------------- |
| `name`       | string    | read-only | OS name.                                                 |
| `version`    | string    | read-only | OS version string.                                       |
| `build_id`   | string    | read-only | Kernel or system build identifier.                       |
| `boot_time`  | timestamp | read-only | Time when the OS booted.                                 |
| `uptime_ns`  | u64       | read-only | System uptime in nanoseconds.                            |
| `hostname`   | string    | read-only | System hostname. Use `set_hostname` method to change it. |
| `machine_id` | uuid      | read-only | Stable machine identifier, if available.                 |

### Methods

| Method             | Arguments    | Result         | Description                                      |
| ------------------ | ------------ | -------------- | ------------------------------------------------ |
| `get_capabilities` | none         | CapabilityList | Returns supported OS capabilities.               |
| `shutdown`         | ShutdownMode | void           | Requests system shutdown. Requires admin rights. |
| `reboot`           | RebootMode   | void           | Requests system reboot. Requires admin rights.   |
| `sync`             | none         | void           | Flushes dirty system state to storage.           |
| `set_hostname`     | string       | void           | Changes system hostname. Requires admin rights.  |

### Events

| Event                | Payload         | Description                  |
| -------------------- | --------------- | ---------------------------- |
| `shutdown_requested` | ShutdownRequest | Fired when shutdown begins.  |
| `reboot_requested`   | RebootRequest   | Fired when reboot begins.    |
| `hostname_changed`   | string          | Fired when hostname changes. |

---

## 5.2 `/os/kernel`

Represents kernel-level state and diagnostic information.

Example path:

```text
/os/kernel
```

### Properties

| Property         | Type   | Access    | Description                            |
| ---------------- | ------ | --------- | -------------------------------------- |
| `kernel_version` | string | read-only | Kernel version.                        |
| `architecture`   | string | read-only | CPU architecture.                      |
| `page_size`      | u64    | read-only | Default memory page size.              |
| `tick_frequency` | u64    | read-only | Kernel timer frequency, if applicable. |
| `panic_count`    | u64    | read-only | Number of recorded kernel panics.      |
| `debug_enabled`  | bool   | read-only | Whether debug facilities are enabled.  |

### Methods

| Method            | Arguments   | Result         | Description                                            |
| ----------------- | ----------- | -------------- | ------------------------------------------------------ |
| `get_symbol_info` | SymbolQuery | SymbolInfo     | Returns kernel symbol info if permitted.               |
| `dump_state`      | DumpOptions | DiagnosticDump | Produces diagnostic state. Requires privileged rights. |
| `set_log_level`   | LogLevel    | void           | Changes kernel log verbosity. Requires admin rights.   |

### Events

| Event       | Payload       | Description                                 |
| ----------- | ------------- | ------------------------------------------- |
| `panic`     | PanicInfo     | Fired when a kernel panic is recorded.      |
| `warning`   | KernelWarning | Fired for significant kernel warnings.      |
| `log_entry` | LogEntry      | Fired for kernel log entries if subscribed. |

---

## 5.3 `/os/processes`

Directory-like branch containing process objects.

Example paths:

```text
/os/processes
/os/processes[1]
/os/processes[42]
/os/processes[42]/threads
/os/processes[42]/memory
```

### Branch Object: `/os/processes`

#### Properties

| Property        | Type | Access    | Description                                   |
| --------------- | ---- | --------- | --------------------------------------------- |
| `count`         | u64  | read-only | Number of live processes.                     |
| `next_pid_hint` | u64  | read-only | Diagnostic hint for next PID. Not stable ABI. |

#### Methods

| Method         | Arguments          | Result        | Description                                    |
| -------------- | ------------------ | ------------- | ---------------------------------------------- |
| `list`         | ProcessListOptions | ProcessList   | Lists visible processes.                       |
| `spawn`        | SpawnRequest       | ProcessHandle | Creates a process.                             |
| `find_by_name` | string             | ProcessList   | Finds processes by executable or service name. |

#### Events

| Event             | Payload         | Description                      |
| ----------------- | --------------- | -------------------------------- |
| `process_created` | ProcessInfo     | Fired when a process is created. |
| `process_exited`  | ProcessExitInfo | Fired when a process exits.      |

### Process Object: `/os/processes/{pid}`

#### Properties

| Property          | Type         | Access    | Description                              |
| ----------------- | ------------ | --------- | ---------------------------------------- |
| `pid`             | u64          | read-only | Process identifier.                      |
| `parent_pid`      | u64          | read-only | Parent process identifier.               |
| `name`            | string       | read-only | Process name.                            |
| `executable_path` | string       | read-only | Path of executable image if known.       |
| `state`           | ProcessState | read-only | Running, sleeping, stopped, zombie, etc. |
| `owner_user_id`   | u64          | read-only | Owning user.                             |
| `cpu_time_ns`     | u64          | read-only | CPU time consumed.                       |
| `memory_bytes`    | u64          | read-only | Resident memory estimate.                |
| `exit_code`       | i64          | read-only | Exit code if exited.                     |

#### Methods

| Method             | Arguments         | Result           | Description                                                     |
| ------------------ | ----------------- | ---------------- | --------------------------------------------------------------- |
| `suspend`          | none              | void             | Suspends the process. Requires rights.                          |
| `resume`           | none              | void             | Resumes the process. Requires rights.                           |
| `terminate`        | TerminateRequest  | void             | Terminates the process. Requires rights.                        |
| `open_memory_view` | MemoryViewRequest | MemoryViewHandle | Opens controlled memory access. Requires debugger/admin rights. |
| `list_handles`     | none              | HandleList       | Lists process handles if permitted.                             |

#### Events

| Event           | Payload          | Description                       |
| --------------- | ---------------- | --------------------------------- |
| `exited`        | ProcessExitInfo  | Fired when the process exits.     |
| `faulted`       | ProcessFaultInfo | Fired when the process faults.    |
| `state_changed` | ProcessState     | Fired when process state changes. |
| `handle_opened` | HandleInfo       | Optional diagnostic event.        |
| `handle_closed` | HandleInfo       | Optional diagnostic event.        |

---

## 5.4 `/os/devices`

Directory-like branch containing discovered hardware and virtual devices.

Example paths:

```text
/os/devices
/os/devices/keyboard[0]
/os/devices/mouse[0]
/os/devices/net[0]
/os/devices/gpu[0]
/os/devices/disk[0]
```

### Branch Object: `/os/devices`

#### Properties

| Property               | Type | Access    | Description                           |
| ---------------------- | ---- | --------- | ------------------------------------- |
| `count`                | u64  | read-only | Number of visible devices.            |
| `enumeration_complete` | bool | read-only | Whether initial enumeration finished. |

#### Methods

| Method          | Arguments         | Result     | Description                                    |
| --------------- | ----------------- | ---------- | ---------------------------------------------- |
| `list`          | DeviceListOptions | DeviceList | Lists visible devices.                         |
| `find_by_class` | DeviceClass       | DeviceList | Lists devices by class.                        |
| `rescan`        | RescanOptions     | void       | Requests device rescan. Requires admin rights. |

#### Events

| Event            | Payload    | Description                      |
| ---------------- | ---------- | -------------------------------- |
| `device_added`   | DeviceInfo | Fired when a device appears.     |
| `device_removed` | DeviceInfo | Fired when a device disappears.  |
| `device_changed` | DeviceInfo | Fired when device state changes. |

### Network Device Object: `/os/devices/net[0]`

#### Properties

| Property       | Type        | Access    | Description                                                   |
| -------------- | ----------- | --------- | ------------------------------------------------------------- |
| `name`         | string      | read-only | Device name.                                                  |
| `device_class` | DeviceClass | read-only | Network device class.                                         |
| `driver`       | string      | read-only | Bound driver name.                                            |
| `mac_address`  | bytes[6]    | read-only | MAC address.                                                  |
| `link_up`      | bool        | read-only | Physical or virtual link state.                               |
| `mtu`          | u32         | read-only | Maximum transmission unit. Use `set_mtu` method to change it. |
| `rx_packets`   | u64         | read-only | Received packet count.                                        |
| `tx_packets`   | u64         | read-only | Transmitted packet count.                                     |
| `rx_bytes`     | u64         | read-only | Received byte count.                                          |
| `tx_bytes`     | u64         | read-only | Transmitted byte count.                                       |

#### Methods

| Method           | Arguments    | Result       | Description                                     |
| ---------------- | ------------ | ------------ | ----------------------------------------------- |
| `configure_ip`   | IpConfig     | void         | Applies IP configuration.                       |
| `set_mtu`        | u32          | void         | Changes maximum transmission unit if permitted. |
| `send_packet`    | PacketBuffer | SendResult   | Sends a raw packet if permitted.                |
| `reset`          | none         | void         | Resets the device. Requires admin rights.       |
| `get_statistics` | none         | NetworkStats | Returns detailed statistics.                    |

#### Events

| Event             | Payload     | Description                                                |
| ----------------- | ----------- | ---------------------------------------------------------- |
| `link_changed`    | LinkState   | Fired when link state changes.                             |
| `packet_received` | PacketInfo  | Fired when packets arrive, depending on subscription mode. |
| `error`           | DeviceError | Fired when device errors occur.                            |

### GPU Device Object: `/os/devices/gpu[0]`

#### Properties

| Property             | Type   | Access    | Description               |
| -------------------- | ------ | --------- | ------------------------- |
| `name`               | string | read-only | GPU name.                 |
| `vendor_id`          | u32    | read-only | Vendor identifier.        |
| `device_id`          | u32    | read-only | Device identifier.        |
| `driver`             | string | read-only | Bound driver name.        |
| `memory_total_bytes` | u64    | read-only | Total GPU-visible memory. |
| `memory_used_bytes`  | u64    | read-only | Used GPU-visible memory.  |
| `queue_count`        | u32    | read-only | Number of exposed queues. |
| `fault_count`        | u64    | read-only | Number of GPU faults.     |

#### Methods

| Method           | Arguments           | Result          | Description                        |
| ---------------- | ------------------- | --------------- | ---------------------------------- |
| `create_buffer`  | GpuBufferCreateInfo | GpuBufferHandle | Creates a GPU buffer object.       |
| `destroy_buffer` | GpuBufferHandle     | void            | Destroys a GPU buffer.             |
| `submit`         | GpuSubmitRequest    | GpuSubmitResult | Submits commands to a GPU queue.   |
| `wait_idle`      | Timeout             | WaitResult      | Waits until GPU work is idle.      |
| `reset`          | none                | void            | Resets GPU. Requires admin rights. |

#### Events

| Event             | Payload               | Description                                        |
| ----------------- | --------------------- | -------------------------------------------------- |
| `faulted`         | GpuFaultInfo          | Fired when GPU faults.                             |
| `memory_pressure` | GpuMemoryPressureInfo | Fired when GPU memory pressure changes.            |
| `work_completed`  | GpuCompletionInfo     | Fired when submitted work completes, if requested. |

---

## 5.5 `/os/drivers`

Branch containing loaded driver objects.

Example paths:

```text
/os/drivers
/os/drivers/virtio_net
/os/drivers/ahci
/os/drivers/simple_framebuffer
```

### Properties

| Property | Type | Access    | Description               |
| -------- | ---- | --------- | ------------------------- |
| `count`  | u64  | read-only | Number of loaded drivers. |

### Driver Object Properties

| Property             | Type        | Access    | Description                             |
| -------------------- | ----------- | --------- | --------------------------------------- |
| `name`               | string      | read-only | Driver name.                            |
| `version`            | string      | read-only | Driver version.                         |
| `state`              | DriverState | read-only | Loaded, active, failed, unloading, etc. |
| `bound_device_count` | u64         | read-only | Number of devices bound to this driver. |
| `provider_path`      | string      | read-only | Path to driver image or provider.       |

### Driver Object Methods

| Method            | Arguments     | Result            | Description                                         |
| ----------------- | ------------- | ----------------- | --------------------------------------------------- |
| `bind`            | DeviceHandle  | void              | Binds the driver to a device. Requires rights.      |
| `unbind`          | DeviceHandle  | void              | Unbinds the driver from a device. Requires rights.  |
| `reload`          | ReloadOptions | void              | Reloads driver if supported. Requires admin rights. |
| `get_diagnostics` | none          | DriverDiagnostics | Returns diagnostic info.                            |

### Driver Object Events

| Event           | Payload           | Description                         |
| --------------- | ----------------- | ----------------------------------- |
| `bound`         | DeviceInfo        | Fired when driver binds a device.   |
| `unbound`       | DeviceInfo        | Fired when driver unbinds a device. |
| `failed`        | DriverFailureInfo | Fired when driver fails.            |
| `state_changed` | DriverState       | Fired when driver state changes.    |

---

## 5.6 `/os/services`

Branch containing long-running service objects.

Example paths:

```text
/os/services
/os/services/logger
/os/services/window_manager
/os/services/network_manager
```

### Branch Properties

| Property | Type | Access    | Description                    |
| -------- | ---- | --------- | ------------------------------ |
| `count`  | u64  | read-only | Number of registered services. |

### Branch Methods

| Method    | Arguments             | Result        | Description                          |
| --------- | --------------------- | ------------- | ------------------------------------ |
| `list`    | ServiceListOptions    | ServiceList   | Lists registered services.           |
| `start`   | ServiceStartRequest   | ServiceHandle | Starts a service. Requires rights.   |
| `stop`    | ServiceStopRequest    | void          | Stops a service. Requires rights.    |
| `restart` | ServiceRestartRequest | void          | Restarts a service. Requires rights. |

### Branch Events

| Event                  | Payload            | Description                       |
| ---------------------- | ------------------ | --------------------------------- |
| `service_registered`   | ServiceInfo        | Fired when a service registers.   |
| `service_unregistered` | ServiceInfo        | Fired when a service unregisters. |
| `service_failed`       | ServiceFailureInfo | Fired when a service fails.       |

### Service Object Properties

| Property            | Type          | Access    | Description                                                           |
| ------------------- | ------------- | --------- | --------------------------------------------------------------------- |
| `name`              | string        | read-only | Service name.                                                         |
| `state`             | ServiceState  | read-only | Starting, running, stopped, failed, etc.                              |
| `pid`               | u64           | read-only | Backing process ID, if any.                                           |
| `restart_policy`    | RestartPolicy | read-only | Service restart policy. Use `set_restart_policy` method to change it. |
| `interface_version` | u32           | read-only | Service interface version.                                            |

### Service Object Methods

| Method               | Arguments         | Result        | Description                                  |
| -------------------- | ----------------- | ------------- | -------------------------------------------- |
| `start`              | none              | void          | Starts the service.                          |
| `stop`               | StopOptions       | void          | Stops the service.                           |
| `restart`            | RestartOptions    | void          | Restarts the service.                        |
| `get_status`         | none              | ServiceStatus | Returns detailed service status.             |
| `open_client`        | ClientOpenRequest | ClientHandle  | Opens a client session to the service.       |
| `set_restart_policy` | RestartPolicy     | void          | Changes service restart policy if permitted. |

### Service Object Events

| Event                 | Payload            | Description                |
| --------------------- | ------------------ | -------------------------- |
| `started`             | ServiceInfo        | Fired when service starts. |
| `stopped`             | ServiceInfo        | Fired when service stops.  |
| `failed`              | ServiceFailureInfo | Fired when service fails.  |
| `client_connected`    | ClientInfo         | Optional diagnostic event. |
| `client_disconnected` | ClientInfo         | Optional diagnostic event. |

---

## 5.7 `/os/memory`

Branch exposing system memory information and controlled memory objects.

Example paths:

```text
/os/memory
/os/memory/physical
/os/memory/virtual
```

### Properties

| Property                   | Type | Access    | Description                  |
| -------------------------- | ---- | --------- | ---------------------------- |
| `page_size`                | u64  | read-only | Default page size.           |
| `total_physical_bytes`     | u64  | read-only | Total physical memory.       |
| `available_physical_bytes` | u64  | read-only | Available physical memory.   |
| `kernel_reserved_bytes`    | u64  | read-only | Kernel-reserved memory.      |
| `user_committed_bytes`     | u64  | read-only | User-space committed memory. |

### Methods

| Method            | Arguments              | Result             | Description                              |
| ----------------- | ---------------------- | ------------------ | ---------------------------------------- |
| `allocate_shared` | SharedMemoryCreateInfo | SharedMemoryHandle | Allocates shared memory object.          |
| `get_statistics`  | none                   | MemoryStatistics   | Returns memory statistics.               |
| `compact`         | CompactOptions         | CompactResult      | Requests memory compaction if supported. |

### Events

| Event                | Payload          | Description                         |
| -------------------- | ---------------- | ----------------------------------- |
| `low_memory`         | LowMemoryInfo    | Fired when memory pressure is high. |
| `statistics_changed` | MemoryStatistics | Optional periodic event.            |

---

## 5.8 `/os/events`

Branch exposing event-related primitives.

Example paths:

```text
/os/events
/os/events/timers
```

### Properties

| Property              | Type | Access    | Description                                             |
| --------------------- | ---- | --------- | ------------------------------------------------------- |
| `subscription_count`  | u64  | read-only | Number of active event subscriptions visible to caller. |
| `timer_resolution_ns` | u64  | read-only | Timer resolution.                                       |

### Methods

| Method               | Arguments            | Result           | Description                      |
| -------------------- | -------------------- | ---------------- | -------------------------------- |
| `create_timer`       | TimerCreateInfo      | TimerHandle      | Creates a timer object.          |
| `create_event_queue` | EventQueueCreateInfo | EventQueueHandle | Creates an event queue.          |
| `wait_many`          | WaitManyRequest      | WaitManyResult   | Waits on multiple event sources. |

### Events

| Event            | Payload                | Description                                             |
| ---------------- | ---------------------- | ------------------------------------------------------- |
| `timer_fired`    | TimerInfo              | Generic timer event if using branch-level subscription. |
| `queue_overflow` | EventQueueOverflowInfo | Fired when an event queue overflows.                    |

---

## 5.9 `/home`, `/bin`, `/lib`, `/tmp`

Storage-backed branches.

These are provided by filesystem providers such as ext2, FAT32, initramfs, tmpfs, or future native filesystems.

### File Object Properties

| Property        | Type        | Access    | Description                                     |
| --------------- | ----------- | --------- | ----------------------------------------------- |
| `name`          | string      | read-only | File name.                                      |
| `size`          | u64         | read-only | File size.                                      |
| `storage_size`  | u64         | read-only | File backend storage size.                      |
| `created_at`    | timestamp   | read-only | Creation time if supported.                     |
| `modified_at`   | timestamp   | read-only | Last modification time.                         |
| `owner_user_id` | u64         | read-only | Owning user. Use method to change it.           |
| `permissions`   | Permissions | read-only | File permissions. Use method to change them.    |
| `content_type`  | string      | read-only | Optional content type. Use method to change it. |
| `data`          | Stream      | read-only | Main byte stream of the file.                   |

### File Object Methods

| Method             | Arguments    | Result     | Description                                         |
| ------------------ | ------------ | ---------- | --------------------------------------------------- |
| `truncate`         | u64          | void       | Changes file size.                                  |
| `sync`             | none         | void       | Flushes file state to storage.                      |
| `clone`            | CloneOptions | FileHandle | Clones file if supported.                           |
| `get_extents`      | none         | ExtentList | Returns storage extents if supported and permitted. |
| `set_owner`        | UserId       | void       | Changes file owner if permitted.                    |
| `set_permissions`  | Permissions  | void       | Changes file permissions if permitted.              |
| `set_content_type` | string       | void       | Changes optional content type if supported.         |

### File Object Events

| Event              | Payload                | Description                                                  |
| ------------------ | ---------------------- | ------------------------------------------------------------ |
| `modified`         | FileChangeInfo         | Fired when file content changes.                             |
| `metadata_changed` | FileMetadataChangeInfo | Fired when metadata changes.                                 |
| `deleted`          | FileDeleteInfo         | Fired before or after file deletion, depending on semantics. |

### Directory Object Properties

| Property             | Type        | Access    | Description                                        |
| -------------------- | ----------- | --------- | -------------------------------------------------- |
| `name`               | string      | read-only | Directory name.                                    |
| `entry_count`        | u64         | read-only | Number of entries if cheaply available.            |
| `storage_size`       | u64         | read-only | Storage size for the directory.                    |
| `file_size`          | u64         | read-only | Recursive sum of file sizes.                       |
| `file_storage_size`  | u64         | read-only | Recursive sum of file backend storage sizes.       |
| `owner_user_id`      | u64         | read-only | Owning user. Use method to change it.              |
| `permissions`        | Permissions | read-only | Directory permissions. Use method to change them.  |
| `children`           | Collection  | read-only | Child files, directories, mounts, or object links. |

### Directory Object Methods

| Method             | Arguments              | Result             | Description                                 |
| ------------------ | ---------------------- | ------------------ | ------------------------------------------- |
| `list`             | DirectoryListOptions   | DirectoryEntryList | Lists entries.                              |
| `lookup`           | string                 | ObjectHandle       | Resolves a child by name.                   |
| `create_file`      | CreateFileRequest      | FileHandle         | Creates a file.                             |
| `create_directory` | CreateDirectoryRequest | DirectoryHandle    | Creates a directory.                        |
| `unlink`           | string                 | void               | Removes an entry.                           |
| `rename`           | RenameRequest          | void               | Renames or moves an entry.                  |
| `mount`            | MountRequest           | void               | Mounts an object provider at a child name.  |
| `set_owner`        | UserId                 | void               | Changes directory owner if permitted.       |
| `set_permissions`  | Permissions            | void               | Changes directory permissions if permitted. |

### Directory Object Events

| Event              | Payload                     | Description                     |
| ------------------ | --------------------------- | ------------------------------- |
| `entry_added`      | DirectoryEntryInfo          | Fired when an entry is added.   |
| `entry_removed`    | DirectoryEntryInfo          | Fired when an entry is removed. |
| `entry_renamed`    | DirectoryRenameInfo         | Fired when an entry is renamed. |
| `metadata_changed` | DirectoryMetadataChangeInfo | Fired when metadata changes.    |

---

## 6. Interface Attribute Categories

Each VFS object interface contains exactly three categories:

- Properties
- Methods
- Events

Additional structures such as streams, memory regions, child branches, maps, dictionaries, arrays, and collections are represented as property types.

## 6.1 Properties

Properties are named, typed values attached to an object.

Properties are externally read-only. Objects are changed by calling methods, not by assigning property values directly.

This means a property may expose a mutable object, while the property binding itself remains read-only. For example, a file's `data` property may return a `Stream<byte>`. The caller cannot replace `file.data`, but may call `file.data.write(...)` if the stream grants write rights.

Properties may be:

- Static
- Dynamic
- Computed
- Privileged
- Object-valued
- Collection-valued
- Stream-valued
- Memory-valued

Example property descriptor:

```text
PropertyDescriptor {
  id: u32
  name: string
  type_id: TypeId
  required_rights: Rights
  flags: cached | dynamic | privileged | volatile | object_ref | collection_ref | stream_ref | memory_ref
}
```

---

## 6.2 Methods

Methods are typed operations callable on an object.

Example method descriptor:

```text
MethodDescriptor {
  id: u32
  name: string
  argument_type: TypeId
  result_type: TypeId
  required_rights: Rights
  flags: may_block | privileged | async | idempotent
}
```

Methods should replace ad-hoc `ioctl`-style command blobs in the native OS model.

---

## 6.3 Events

Events are typed notifications emitted by objects.

Example event descriptor:

```text
EventDescriptor {
  id: u32
  name: string
  payload_type: TypeId
  required_rights: Rights
  flags: edge_triggered | level_triggered | queued | lossy
}
```

Event delivery should be unified through subscription handles and wait operations.

---

## 6.4 Standard Library Interface Types

Streams, memory regions, collections, dictionaries, and maps are modeled as normal typed objects from class libraries, not as separate interface descriptor categories.

The standard class library should provide common reusable types such as:

```text
List<T>
Collection<T>
Dictionary<K, V>
Map<K, V>
Stream<T>
MemoryStream
MemoryArray
ObjectRef<T>
```

Example stream-like type:

```text
class Stream<T> {
  properties:
    position: UInt64
    length: Optional<UInt64>
    readable: Bool
    writable: Bool
    seekable: Bool

  methods:
    read(ReadRequest) -> ReadResult<T>
    write(WriteRequest<T>) -> WriteResult
    seek(SeekRequest) -> u64
    flush()
    close()

  events:
    data_available(StreamDataInfo)
    closed(StreamCloseInfo)
    error(StreamErrorInfo)
}
```

Example collection type:

```text
class Collection<T> {
  properties:
    count: u64

  methods:
    list(ListOptions) -> List<T>
    get(CollectionKey) -> Optional<T>
    contains(CollectionKey) -> Bool

  events:
    added(T)
    removed(T)
    changed(T)
}
```

Example memory types:

```text
class MemoryStream : Stream<byte> {
  methods:
    map(MapRequest) -> MemoryMapping
    protect(ProtectionRequest) -> Result<Void>
}

class MemoryArray {
  properties:
    length: u64
    element_size: u64

  methods:
    get(index: u64) -> MemoryStream
    map(index: u64, request: MapRequest) -> MemoryMapping
}
```

---

## 6.5 Memory Region Modeling

Memory regions are not descriptor categories. They are properties whose types come from the standard class library or an OS-level memory class library.

Examples:

- A process may expose `memory_regions: Collection<MemoryStream>`.
- A framebuffer device may expose `framebuffer: MemoryStream`.
- A fixed hardware register bank may expose `registers: MemoryArray`.
- A file may expose `data: Stream<byte>` and may also support a `map` method through the stream implementation.

Use `Collection<MemoryStream>` when the number of regions can change.

Use `MemoryArray` when the layout is fixed and indexed.

Memory access rights are enforced by the returned memory object and its methods, not by a separate VFS memory-region operation category.

---

## 7. Rights Model

Access control should be operation-level, not only path-level.

Suggested rights:

```text
RIGHT_DESCRIBE
RIGHT_GET_PROPERTY
RIGHT_CALL_METHOD
RIGHT_SUBSCRIBE_EVENT
RIGHT_ENUMERATE_COLLECTION
RIGHT_CREATE_COLLECTION_ITEM
RIGHT_DELETE_COLLECTION_ITEM
RIGHT_ADMIN
RIGHT_DELEGATE
```

A handle carries a subset of rights granted at open time or delegated from another process.

Example:

```text
handle to /os/devices/gpu0:
  DESCRIBE
  GET_PROPERTY
  CALL_METHOD:create_buffer
  CALL_METHOD:submit
  SUBSCRIBE_EVENT:work_completed
```

Fine-grained method/property/event rights may later be represented by per-interface-item permission masks.

---

## 8. Object Lifetime

Objects may be:

- Persistent and path-backed
- Runtime and path-backed
- Runtime and handle-only
- Ephemeral event or result objects

Examples:

| Object             | Path-backed | Persistent | Notes                                                       |
| ------------------ | ----------- | ---------- | ----------------------------------------------------------- |
| File               | yes         | yes        | Backed by storage.                                          |
| Directory          | yes         | yes        | Backed by storage or object provider.                       |
| Process            | yes         | no         | Exists while process exists.                                |
| GPU buffer         | usually no  | no         | Usually handle-only.                                        |
| Event subscription | no          | no         | Handle-only.                                                |
| Service            | yes         | no/maybe   | Runtime service, optionally configured by persistent files. |
| Shared memory      | optional    | no/maybe   | May be named or anonymous.                                  |

---

## 9. Provider Model

A VFS branch is backed by an object provider.

A branch is normally represented as an object with a `children` property of type `Collection<T>`. Directory traversal is therefore a method call on a collection object, not a separate descriptor category.

Examples:

- ext2 provider
- FAT32 provider
- tmpfs provider
- process manager provider
- device manager provider
- service manager provider
- driver manager provider
- kernel diagnostics provider

A provider implements object lookup and object operations for a namespace subtree.

Conceptual provider interface:

```text
Provider {
  open(path, requested_rights) -> object_handle
  describe(object) -> interface_descriptor
  get_property(object, property_id) -> value
  call_method(object, method_id, args) -> result
  subscribe_event(object, event_id) -> subscription_handle
}
```

Lookup, listing, stream I/O, and memory mapping are still supported, but through normal object interfaces:

```text
directory.children.get(name)
directory.children.list(options)
file.data.read(request)
file.data.write(request)
process.memory_regions.get(index).map(request)
```

---

## 10. POSIX Compatibility Projection

POSIX compatibility should be implemented as a layer over native object handles.

Mapping:

| POSIX concept       | Native object model                                      |
| ------------------- | -------------------------------------------------------- |
| file descriptor     | object handle                                            |
| `open`              | `open`                                                   |
| `read`              | get `data: Stream<byte>` property, call `read` method    |
| `write`             | get `data: Stream<byte>` property, call `write` method   |
| `stat`              | property query                                           |
| `opendir`           | open directory object                                    |
| `readdir`           | call directory `list` method                             |
| `ioctl`             | legacy wrapper over `call_method`                        |
| `mmap`              | call `map` on `MemoryStream` or mappable `Stream` object |
| `poll/select/epoll` | event subscription and `wait_many`                       |

The native model should not be reduced to POSIX semantics.

---

## 11. Minimal Implementation Milestones

### Milestone 1: Kernel Object Base

Implement:

- `Object`
- `ObjectKind`
- `Handle`
- `Rights`
- Basic object table per process

Required operations:

```text
open
close
info
```

---

### Milestone 2: Directory and File Objects

Implement:

- Root directory
- Directory lookup
- Directory list
- Basic file object
- Read-only `data` property of type `Stream<byte>` for file data

---

### Milestone 3: `/os` Runtime Namespace

Implement synthetic runtime branches:

```text
/os/system
/os/processes
/os/devices
```

Expose basic read-only properties.

---

### Milestone 4: Properties

Add generic:

```text
get_property
```

Use properties for:

- System info
- Process state
- Device state
- File metadata
- File `data: Stream<byte>`
- Directory `children: Collection<FileSystemObject>`

---

### Milestone 5: Methods

Add generic:

```text
call_method
```

Use methods for:

- Stream `read` and `write`
- Process `terminate`
- Directory `children.get` / `children.list`, or direct `lookup` / `list` while bootstrapping
- Directory `create_file`, `create_directory`, `unlink`
- Device `reset`
- Service `start` and `stop`

---

### Milestone 6: Events

Add:

```text
subscribe_event
unsubscribe_event
wait
wait_many
```

Use events for:

- Process exit
- Timer fired
- Device change
- File or directory change
- Stream `data_available`

---

## 12. Open Design Questions

1. Should object interfaces be described by compact binary descriptors, text schemas, or both?
2. Should method/property/event IDs be globally stable, per-interface stable, or dynamically assigned at load time?
3. Should all objects expose a minimum common interface?
4. How much type information should the kernel understand?
5. Should the kernel validate method argument schemas, or only route opaque typed payloads?
6. Should in-process loaded ELF objects use the same descriptor format as out-of-process services?
7. How should object references be serialized across IPC boundaries?
8. Should there be a native object query language, or only directory traversal and explicit method calls?
9. How should POSIX compatibility handle non-file objects?
10. Which branches are required for the first bootable milestone?
11. Which parts of the standard class library are kernel ABI and which are OS runtime ABI?
12. Should `Collection<T>` expose mutation directly, or should all mutations happen on the owning object through explicit methods?

---

## 13. Recommended First Version

The first version should be deliberately small.

Recommended initial native object kinds:

```text
Directory
File
System
Process
Device
Service
Stream
Collection
EventSubscription
```

Recommended initial operations:

```text
open
close
describe_basic
get_property
call_method
subscribe_event
wait
```

Initial `File` objects should expose `data: Stream<byte>`. Initial `Directory` objects should expose `children: Collection<FileSystemObject>`. Reading, writing, listing, lookup, and traversal then become method calls on those standard objects.

The critical early decision is to make VFS resolve paths to object handles, not merely file descriptors. Everything else can evolve from that foundation.
