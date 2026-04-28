# os1 Shell Language — First Draft

## Status

Draft for iteration.

This document proposes a first draft of the native `os1` shell / scripting language. The language may eventually support both interpreted and ahead-of-time compiled execution. This draft defines the **semantic model first** and leaves parser/runtime implementation strategy flexible.

The language is intended to be:

- concise enough for interactive shell use
- structured enough for reliable automation
- typed enough for tooling, reflection, and composition
- honest about process, object, and isolation boundaries
- suitable as a native interface **beyond POSIX**, while preserving a path to POSIX compatibility

---

## 1. Design goals

The language should:

- support small interactive shell commands
- support larger scripts and automation workflows
- expose `os1` native concepts directly
- keep local data and hosted OS objects distinct
- provide synchronous and asynchronous invocation models
- support properties, methods, and events as first-class syntax
- allow exception-based error handling with compact syntax
- preserve strong observability and control of the running system
- support tooling through discoverable metadata and typed interfaces

The language should **not** become:

- a text-only POSIX shell clone
- a giant general-purpose programming language too early
- a distributed-object fantasy that hides OS boundaries
- a GUI scripting system before the core model is coherent

---

## 2. Core semantic model

The language has four major runtime categories:

### 2.1 Value types

Examples:

- integers
- booleans
- strings
- byte arrays
- enums
- tuples
- records / structs
- arrays

These are local to the current process and passed by value unless otherwise specified.

### 2.2 Local objects

Examples:

- script-local collections
- local helper objects
- counters
- local state holders
- compiler/runtime-generated closures

These live in the script process address space.

### 2.3 Hosted objects

Hosted objects are process-backed OS objects loaded or obtained through the system.

Examples:

- `os.load("/bin/ls")`
- `os.load("/bin/crop")`
- future device/service objects

Hosted objects:

- live in protected child processes or equivalent hosted contexts
- are referenced through handles
- preserve state across method calls
- may expose properties, methods, and events
- can emit events to subscribed callers
- die when their parent process dies unless later transferred or detached by explicit future policy

### 2.4 Handles

A handle is an owned reference to an object.

Properties of handles:

- reference counted
- scoped by language lifetime rules
- may refer to local or hosted objects
- support multiple aliases
- determine lifetime of hosted child objects
- are the unit of access, invocation, and subscription

---

## 3. Execution and isolation model

### 3.1 Script process

A shell command or script runs in a script process.

This process owns:

- local values
- local objects
- handles to hosted objects
- event handlers
- the ambient `os` object

### 3.2 Hosted child process model

`os.load(...)` creates a **hosted child object** backed by a protected child process.

Consequences:

- constructor / initialization executes at load time
- object state persists until destruction
- method calls are process-boundary calls, even if syntax looks object-oriented
- events are delivered across process boundaries
- if the script process dies, its hosted child objects die too by default

The model is recursive: hosted objects may create further hosted objects, subject to security and ownership rules.

### 3.3 Honesty rule

The language may provide ergonomic syntax, but the design must remain honest:

- hosted method calls are not raw in-process function calls
- hosted property access may imply IPC / messaging
- event delivery crosses protection boundaries
- exceptions crossing hosted boundaries are serialized failures re-raised by the caller runtime

---

## 4. Language style

The language should support both:

- compact shell-like usage
- structured script/program style

Examples:

```text
ls = os.load("/bin/ls")
ls_arg : type(ls.arg)
ls_arg.input.mask += "*.jpg"
ls.run(ls_arg)
```

```text
for item in ls.items({ mask: "*.jpg" }) {
  println(item.path)
}
```

---

## 5. Type system and metadata

### 5.1 Goals

The type system should be:

- discoverable
- tool-friendly
- compact enough for scripts
- strong enough to describe object interfaces

### 5.2 Interface discovery

Program metadata is discoverable from loadable program images.

Compiler/runtime may inspect metadata to determine:

- object type
- properties
- methods
- events
- argument types
- return types
- exceptions
- version / compatibility metadata

### 5.3 Type query syntax

This draft accepts:

```text
ls_arg : type(ls.arg)
```

Meaning:

- `ls.arg` refers to a discoverable interface member/type descriptor
- `type(...)` resolves the concrete type
- `ls_arg` is declared with that type

Possible future sugar may exist, but this is acceptable in the first draft.

### 5.4 Structural data

Structured values should support:

- named fields
- array fields
- nested records
- literals

Example:

```text
{
  file: item.path
  dimension: { width: 1024; height: 768 }
  preserve_aspect_ratio: true
}
```

---

## 6. Object lifecycle

### 6.1 Loading

The preferred syntax is:

```text
obj = os.load("/bin/name")
```

`os.load(path)`:

- resolves a program image or loadable object source
- creates a hosted object instance
- executes initialization / constructor logic
- returns a handle to the hosted object

### 6.2 Lifetime

Object lifetime is determined by:

- handle scope
- reference count
- process ownership tree

Default policy:

- handle destruction decrements reference count
- final release destroys the hosted object
- parent script death destroys child hosted objects

### 6.3 Multiple references

Multiple references are allowed.

Example:

```text
a = os.load("/bin/ls")
b = a
```

The hosted object survives until both references are gone.

### 6.4 Cleanup

Cleanup should be deterministic where possible.

Future explicit operations may include:

- `obj.close()`
- `obj.dispose()`
- ownership transfer / detach semantics

These are not required for first implementation but should remain possible.

---

## 7. Methods

### 7.1 Synchronous methods

A synchronous method runs to completion before returning.

Example:

```text
crop.run({
  file: item.path
  dimension: { width: 1024; height: 768 }
})
```

Default rule:

- `.run(...)` is synchronous
- exceptions abort the current block and bubble outward

### 7.2 Asynchronous methods

Async methods should be explicit, not overloaded onto sync names.

Examples of acceptable future forms:

- `.start(...)`
- `.run_async(...)`
- `.begin(...)`

Async methods should return a job/task object.

### 7.3 Methods are real interface members

Hosted objects are not limited to `run`.

Example future methods:

- `crop.configure(...)`
- `crop.run(...)`
- `crop.cancel()`
- `crop.stats()`

---

## 8. Properties

### 8.1 Property model

Properties are first-class members of objects.

Kinds:

- read-only
- writable
- array/collection
- computed
- remote-backed

### 8.2 Property access

Examples:

```text
ls_arg.input.mask += "*.jpg"
job.state
device.name
os.process.id
```

### 8.3 Remote property honesty

For hosted objects, property access may imply runtime communication with the hosted process.

Tooling and docs should distinguish:

- cheap local properties
- cached remote properties
- uncached remote properties

### 8.4 Property sugar

Properties may map internally to getter/setter methods.

User-visible syntax should remain simple.

---

## 9. Events

### 9.1 Event model

Objects may expose events.

Examples:

- `on_item`
- `on_complete`
- `on_error`
- `on_progress`
- `on_exit`
- `on_state_changed`

### 9.2 Subscribe

Draft syntax:

```text
handler = ls.on_item += (item) {
  println(item.path)
}
```

`+=` returns a subscription object handle.

### 9.3 Unsubscribe

Supported forms:

```text
ls.on_item -= handler
ls.on_item.clear
count = ls.on_item.size

sub = ls.on_item += (item) { ... }
sub.remove()
```

### 9.4 Subscription object

A subscription object should support at least:

- `remove()`
- `enabled`
- `event`
- `target`

### 9.5 Delivery model

For synchronous hosted calls:

- events emitted during a call are delivered synchronously to the caller runtime
- event ordering must be preserved within one emitter unless documented otherwise
- handler exceptions abort current dispatch and bubble unless caught

### 9.6 Event failure policy

Default proposed rule:

- exception in handler halts current handler block
- if uncaught, it bubbles to the current caller
- if it escapes all caller levels, the shell / OS catches and reports it

### 9.7 Reentrancy

Reentrant calls back into the same hosted object during active event dispatch are dangerous and should be restricted or explicitly defined later.

First-draft rule:

- do not assume unrestricted reentrancy is safe
- runtime may reject unsupported reentrant patterns

---

## 10. Errors and exceptions

### 10.1 Model

The language uses exception propagation as the primary failure model for synchronous operations.

Rules:

- exception halts current block
- exception bubbles to the caller
- uncaught exceptions propagate outward until handled
- if unhandled at top level, shell/OS reports them

### 10.2 Compact handling syntax

Baseline syntax:

```text
try {
  crop.run(...)
} catch e {
  println(e.message)
}
```

Compact statement form should also be considered:

```text
crop.run(...) catch e {
  println(e.message)
}
```

### 10.3 Hosted call exception transport

When a hosted object call fails:

- callee-side failure is captured and serialized
- caller runtime reconstructs and raises a corresponding exception object
- exception appears to bubble naturally in the caller language model

### 10.4 Exception object shape

Exception objects should contain at least:

- type/category
- message
- source object/program
- optional code
- optional nested cause
- optional structured details payload

---

## 11. Control flow

The language should support basic structured control flow:

- `if`
- `else`
- `for`
- `while`
- `return`
- `break`
- `continue`
- `try`
- `catch`

Later possibilities:

- `defer`
- `await`
- `select`
- pattern matching

Not all are required for first implementation.

---

## 12. Native system innovation vs POSIX compatibility

The shell language is **not** intended to be a POSIX shell clone.

Instead:

- the native system model is object-oriented in structure
- programs/services may be represented as hosted objects
- methods, properties, and events are first-class

However, POSIX compatibility remains important.

Strategy:

- POSIX compatibility is preserved where practical
- the native shell language builds on `os1` native object semantics
- POSIX-style process execution may be exposed through `os.spawn(...)` and related APIs
- text streams and file descriptors remain available for compatibility scenarios
- native structured APIs are preferred over text parsing for new automation

---

## 13. The `os` object

## 13.1 Purpose

`os` is the built-in ambient system object visible to every script.

It represents the script’s current system context and provides access to:

- process and job control
- object loading and spawning
- filesystems and namespaces
- users, security, permissions
- devices
- networking
- timing
- observability
- system-wide event sources
- introspection and control

The `os` object is the primary bridge between the script and the operating system.

## 13.2 Design principles for `os`

The `os` object should be:

- comprehensive
- discoverable
- typed
- observable
- script-friendly
- honest about privilege boundaries
- compatible with shell use and larger automation programs

It should avoid:

- excessive fragmentation into obscure root objects too early
- hidden privilege escalation
- stringly-typed ad hoc commands where typed structure is possible

---

## 14. `os` object API — top-level properties

This section lists the first-draft conceptual API, not a frozen ABI.

### 14.1 `os.process`

The current process object.

Suggested properties:

- `id`
- `parent_id`
- `session_id`
- `user`
- `group`
- `state`
- `path`
- `args`
- `env`
- `cwd`
- `started_at`
- `uptime`
- `children`
- `handles`
- `threads`
- `exit_requested`
- `capabilities`
- `security_context`

Suggested methods:

- `exit(code = 0)`
- `kill(signal_or_reason)`
- `wait()`
- `spawn(path, args = null)`
- `load(path)`
- `open(path, mode = null)`
- `observe()`
- `trace(...)`
- `sleep(duration)`

Suggested events:

- `on_child_start`
- `on_child_exit`
- `on_signal`
- `on_exception`
- `on_handle_leak_detected` (future)
- `on_security_violation` (future)

### 14.2 `os.user`

The current user context.

Properties:

- `id`
- `name`
- `groups`
- `home`
- `shell`
- `capabilities`
- `authenticated`
- `auth_method`
- `security_labels` (future)

Methods:

- `can(action, target)`
- `has(capability)`
- `resolve(name_or_id)`
- `switch(...)` (privileged / future)
- `authenticate(...)` (future)

Events:

- `on_reauthenticated`
- `on_privilege_changed`

### 14.3 `os.env`

Environment and execution settings.

Properties:

- `vars`
- `path`
- `locale`
- `timezone`
- `terminal`
- `interactive`
- `debug`
- `trace_enabled`

Methods:

- `get(name)`
- `set(name, value)`
- `unset(name)`

### 14.4 `os.fs`

Filesystem and namespace access.

Properties:

- `cwd`
- `root`
- `mounts`
- `temp`
- `devices_path`
- `services_path`

Methods:

- `open(path, mode = null)`
- `exists(path)`
- `stat(path)`
- `list(path, options = null)`
- `mkdir(path, options = null)`
- `remove(path)`
- `move(from, to)`
- `copy(from, to)`
- `watch(path, options = null)`
- `mount(...)` (privileged)
- `unmount(...)` (privileged)

Events:

- `on_mount`
- `on_unmount`
- `on_namespace_changed`
- `on_watch_event`

### 14.5 `os.net`

Networking access and system network state.

Properties:

- `interfaces`
- `routes`
- `dns`
- `hostname`
- `sockets`
- `online`
- `firewall_state` (future)

Methods:

- `connect(...)`
- `listen(...)`
- `resolve(name)`
- `ping(...)` (future)
- `interface(name_or_id)`
- `route(...)`
- `socket(...)`

Events:

- `on_interface_up`
- `on_interface_down`
- `on_address_changed`
- `on_route_changed`
- `on_dns_changed`
- `on_link_state_changed`

### 14.6 `os.devices`

Device discovery and control.

Properties:

- `all`
- `pci`
- `usb`
- `block`
- `net`
- `display`
- `input`
- `gpu`
- `timers`
- `buses`

Methods:

- `find(filter)`
- `get(id_or_path)`
- `enumerate()`
- `rescan(bus = null)`
- `observe(device)`
- `claim(...)` (privileged/future)
- `release(...)` (privileged/future)

Events:

- `on_device_added`
- `on_device_removed`
- `on_device_changed`
- `on_bus_rescan`
- `on_device_fault`

### 14.7 `os.time`

Timing and scheduling support.

Properties:

- `now`
- `uptime`
- `boot_time`
- `monotonic`
- `timezone`

Methods:

- `sleep(duration)`
- `after(duration, callback = null)`
- `every(duration, callback = null)`
- `deadline(timestamp, callback = null)`
- `measure(block_or_job)`

Events:

- `on_tick` (optional / low-level)
- `on_timer`

### 14.8 `os.observe`

Observability and tracing.

Properties:

- `enabled`
- `level`
- `targets`
- `buffers`
- `snapshots`
- `trace_sessions`

Methods:

- `snapshot(kind = "default")`
- `trace_start(options = null)`
- `trace_stop(session = null)`
- `subscribe(source, filter = null)`
- `events(filter = null)`
- `metrics(filter = null)`
- `inspect(target)`
- `dump(target, options = null)`

Events:

- `on_trace_event`
- `on_snapshot_ready`
- `on_metric_threshold`
- `on_fault_report`
- `on_state_change`

### 14.9 `os.sys`

System-wide information and control.

Properties:

- `name`
- `version`
- `build`
- `boot_info`
- `arch`
- `platform`
- `cpu_topology`
- `memory`
- `power`
- `features`
- `compatibility`

Methods:

- `shutdown(mode = "poweroff")`
- `reboot(mode = "normal")`
- `panic(message)` (privileged/debug)
- `gc()` (if ever relevant to language runtime)
- `feature(name)`
- `compat(name)`
- `log(...)`

Events:

- `on_shutdown_requested`
- `on_low_memory`
- `on_power_state_changed`
- `on_cpu_topology_changed` (future)
- `on_thermal_event` (future)

---

## 15. `os` object API — core methods

### 15.1 `os.load(path, options = null)`

Loads a hosted program object from a path-like identifier.

Input:

- `path`: program image or loadable object path
- optional load options:
  - constructor args
  - security policy
  - environment overrides
  - resource limits
  - trace flags

Returns:

- handle to hosted object

Behavior:

- resolves path
- validates target
- creates hosted child object
- runs constructor
- returns handle

Potential exceptions:

- not found
- invalid image
- incompatible interface
- permission denied
- constructor failure
- resource exhaustion

### 15.2 `os.spawn(path, args = null, options = null)`

POSIX-compatible or one-shot execution-oriented process creation.

Use when persistent object semantics are not required.

Returns:

- process/job object

### 15.3 `os.open(path, mode = null, options = null)`

Open a path-resolved OS object, file, directory, device, channel, or service endpoint as appropriate.

### 15.4 `os.find(query, options = null)`

Find objects/services/devices/processes by structured query.

Future examples:

```text
gpu = os.find({ kind: "device"; class: "gpu"; index: 0 })
shell = os.find({ kind: "service"; name: "shell" })
```

### 15.5 `os.connect(target, options = null)`

Connect to an existing service or endpoint rather than creating a new hosted object.

### 15.6 `os.observe(target = null, options = null)`

Attach observability to a process, object, device, or subsystem.

Returns:

- observation/session object

### 15.7 `os.sleep(duration)`

Suspend current script/process execution for a duration.

### 15.8 `os.require(feature_or_capability)`

Assert that a capability or feature exists.

Throws if missing.

### 15.9 `os.import(module_or_package)`

Future module/package import for script libraries.

---

## 16. `os` object API — process observability and control

The language should allow full observability and strong process control.

### Properties to expose

For individual process objects:

- `id`
- `parent`
- `children`
- `path`
- `cmdline`
- `state`
- `exit_code`
- `threads`
- `handles`
- `cpu_time`
- `memory`
- `io`
- `user`
- `group`
- `session`
- `exceptions`
- `security_context`

### Methods to expose

- `suspend()`
- `resume()`
- `kill(reason = null)`
- `wait()`
- `observe()`
- `snapshot()`
- `stack()` (privileged/debug)
- `threads()`
- `handles()`
- `memory_map()` (privileged/debug)
- `permissions()`
- `children()`

### Events to expose

- `on_start`
- `on_exit`
- `on_exception`
- `on_state_change`
- `on_child_added`
- `on_child_removed`
- `on_security_fault`
- `on_resource_limit_hit`
- `on_deadlock_suspected` (future)
- `on_hang_suspected` (future)

---

## 17. `os` object API — device observability and control

To support real hardware, virtualization, and future accelerators, the shell language should be able to inspect and observe devices.

### Device properties

- `id`
- `name`
- `vendor`
- `device_id`
- `class`
- `subclass`
- `bus`
- `location`
- `driver`
- `resources`
- `interrupts`
- `state`
- `power_state`
- `capabilities`
- `errors`
- `queues` (for modern devices)
- `topology`

### Device methods

- `observe()`
- `rescan()`
- `driver.bind(...)` (privileged/future)
- `driver.unbind()` (privileged/future)
- `reset()` (privileged)
- `enable()`
- `disable()`
- `properties()`
- `events()`
- `metrics()`

### Device events

- `on_online`
- `on_offline`
- `on_reset`
- `on_error`
- `on_driver_bound`
- `on_driver_unbound`
- `on_interrupt`
- `on_queue_event`
- `on_property_changed`

---

## 18. `os` object API — observability-first design

Observability is a first-class shell concern.

The language should be able to access structured system state without parsing text logs.

Examples of observability targets:

- processes
- threads
- schedulers
- CPU topology
- memory allocators
- devices
- block I/O
- networking
- event queues
- exceptions
- traces
- metrics

### Example usage

```text
obs = os.observe(os.process)
snap = obs.snapshot()
println("process {} has {} children", snap.id, snap.children.size)
```

```text
sub = os.observe.on_trace_event += (ev) {
  println("[{}] {}", ev.source, ev.message)
}
```

### Requirements

- structured snapshots
- event subscriptions
- inspectable metrics
- no forced log scraping
- privilege-aware access

---

## 19. Security and permissions in the language

The shell language must not assume universal privilege.

Important concepts:

- current user identity
- capability-based checks
- per-object rights
- namespace permissions
- event subscription permissions
- device/process observability permissions

Example:

```text
if !os.user.can("observe", target) {
  println("permission denied")
  return
}
```

Security should be explicit in:

- `os.load`
- `os.open`
- `os.connect`
- observability APIs
- device control APIs
- process control APIs

---

## 20. POSIX compatibility mapping

The shell language should coexist with POSIX compatibility rather than replace it.

Mapping ideas:

- process-backed hosted objects can still correspond to POSIX processes
- `os.spawn` and process objects cover classic exec-style workflows
- files and directories remain path-based objects
- streams / descriptors remain available
- POSIX shell compatibility, if added, should be layered and not dictate native semantics

---

## 21. Example scripts

### 21.1 Persistent hosted-object example

```text
ls = os.load("/bin/ls")
crop = os.load("/bin/crop")

ls_arg : type(ls.arg)
ls_arg.input.mask += "*.jpg"

cropped = 0
failed = 0

sub = ls.on_item += (item) {
  crop.run({
    file: item.path
    dimension: {1024, 768}
    preserve_aspect_ratio: true
  }) catch e {
    println("crop failed for {}: {}", item.path, e.message)
    failed++
    return
  }

  cropped++
}

ls.run(ls_arg)
println("{} files cropped, {} failed.", cropped, failed)
```

### 21.2 Process observation

```text
obs = os.observe(os.process)

obs.on_state_change += (s) {
  println("process state: {}", s.state)
}
```

### 21.3 Device discovery

```text
gpu = os.devices.find({ class: "gpu"; index: 0 })
if gpu != null {
  println("gpu driver: {}", gpu.driver.name)
}
```

### 21.4 POSIX-style one-shot execution

```text
job = os.spawn("/bin/ls", { path: "/tmp" })
job.wait()
println("exit: {}", job.exit_code)
```

---

## 22. Open design questions

These are still unresolved and need iteration:

- interpreted first, compiled first, or dual model?
- exact syntax for imports/modules
- whether remote property access should be visually marked in some cases
- how async syntax should look
- whether event handlers run on the caller thread or runtime event loop
- reentrancy rules for hosted-object callbacks
- exact security model for subscriptions and observability
- cycle handling with reference counting and subscriptions
- how much reflection is available at runtime
- versioning strategy for program metadata and interface compatibility

---

## 23. Recommended implementation order

1. local values/objects, declarations, blocks, `if`, `for`, `return`
2. built-in `os` object with minimal `load`, `spawn`, `open`, `sleep`
3. hosted object handles
4. synchronous method calls
5. properties
6. events and subscriptions
7. exception propagation
8. runtime metadata/type reflection
9. observability APIs
10. async jobs and future syntax sugar
11. compiler / optimizer / packaging strategy

---

## 24. Short thesis

The native `os1` shell language should be a typed scripting/programming language centered on a first-class `os` object, local values, hosted process-backed objects, methods, properties, and events. It should enable full system observability and control without hiding protection boundaries, while remaining concise enough for shell usage and compatible with a staged POSIX compatibility story.
