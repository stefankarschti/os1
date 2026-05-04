# Native Object Kernel Contract

## Status

Draft for iteration.

This document defines the kernel-level contract for `os1` native objects. It sits between:

- the long-term project goals in `GOALS.md`
- object/interface metadata embedded in loadable images
- the user-facing native shell language and runtime
- the eventual POSIX compatibility layer

The purpose of this document is to define the **kernel substrate**, not the shell syntax.

## 1. Purpose

`os1` aims to expose a native operating-system interface that is not conceptually limited by POSIX, while still allowing POSIX compatibility where useful.

The native interface is built around:

- kernel-managed objects
- rights-bearing handles
- uniform access to heterogeneous interfaces
- structured properties, methods, and events
- synchronous and asynchronous interaction
- structured failure and exception transport
- explicit lifetime and ownership rules

This document defines the kernel contract that makes those features possible.

## 2. Scope

This document defines:

- object and handle semantics
- rights and permission model at the handle level
- interface discovery model
- property, method, and event operations
- synchronous and asynchronous interaction model
- failure and exception transport across kernel and process boundaries
- lifetime and ownership rules
- hosted vs in-process object-loading models
- namespace and lookup implications
- relationship to POSIX compatibility

This document does **not** define:

- shell language syntax
- parser/compiler implementation
- binary encoding details of interface metadata
- full VFS design
- full socket API design
- full security policy beyond the kernel object contract

## 3. Design principles

The native object contract should be:

- uniform at the syscall boundary
- honest about process and protection boundaries
- expressive enough for methods, properties, and events
- compatible with both kernel-native and user-hosted objects
- evolvable without locking the kernel into a fragile ABI too early
- suitable as the substrate for a later POSIX compatibility layer

The contract should avoid:

- baking shell-language syntax into the kernel ABI
- assuming every property is safe to memory-map directly
- using raw cross-process callback pointers as the event ABI
- coupling the kernel contract too tightly to ELF-specific details
- forcing every object to behave as a POSIX file descriptor internally

## 4. Terms and model

### 4.1 Object image

An object image is a loadable unit that may include native-interface metadata.

Examples:

- ELF image with `.os.interface` metadata
- future object/package formats

An object image is not yet a live object.

### 4.2 Object instance

An object instance is a runtime realization of an object image or kernel-native object provider.

An object instance may expose:

- properties
- methods
- events
- type and version metadata

### 4.3 Object kinds

The kernel contract should initially recognize at least the following conceptual object kinds:

- `KernelObject`
- `HostedObject`
- `InProcessObject`
- `ProcessObject`
- `ThreadObject`
- `ProgramImageObject`
- `DeviceObject`
- `ObserveObject`
- `JobObject`
- `SubscriptionObject`
- `NamespaceObject`

These are conceptual categories. The exact kernel type hierarchy may differ.

### 4.4 Handle

A handle is a process-owned reference to an object instance.

A handle is:

- reference counted
- rights-bearing
- scoped to process lifetime unless explicitly transferred
- the unit through which callers access properties, methods, events, and metadata

The kernel ABI should be defined in terms of handles, not raw pointers.

## 5. Object loading models

### 5.1 Hosted object load

A hosted object load creates or accesses an object instance backed by a protected process or equivalent protected execution context.

Properties:

- constructor / initialization runs at load time
- object state persists across method calls
- access happens through handle-mediated calls
- properties, methods, and events may cross protection boundaries
- default lifetime is tied to the owning process unless transferred or published through a service mechanism

This is the primary model for native service-like objects.

### 5.2 In-process object load

An in-process object load creates an object instance inside the caller process.

Properties:

- not inherently accessible from other processes
- lower overhead
- weaker isolation
- suitable for library-like or script-runtime-local objects

This model must remain distinct from hosted objects in semantics, even if language syntax hides some differences.

### 5.3 Kernel-native objects

The kernel may expose object instances that do not originate from user-provided images.

Examples:

- operating system object
- process objects
- device objects
- observability objects
- namespace objects

The `os` object used by native shell examples should be understood as a kernel-native object instance exposed through the same general object model.

## 6. Interface metadata and discovery

When an object image is loaded or otherwise accessed through the kernel, its interface metadata may be parsed and exposed to the caller according to permissions.

This should enable callers to:

- enumerate interface items
- inspect property definitions
- inspect method signatures
- inspect event definitions and payload schemas
- discover required rights for interface members
- discover version and compatibility metadata
- inspect structured exception/failure schemas where available

The kernel contract must support heterogeneous interfaces, but access to them must happen through uniform kernel operations.

The kernel contract must not assume ELF forever, even if ELF is the first metadata transport.

## 7. Rights model

Handles carry rights.

Initial conceptual rights should include:

- `inspect`
- `read_property`
- `write_property`
- `call`
- `subscribe`
- `observe`
- `control`
- `duplicate`
- `transfer`

Possible later refinements:

- per-method rights
- per-property rights
- delegation-limited duplicate/transfer rights
- rights attenuation on transfer

Rights are attached to handles, not to object identity alone.

## 8. Uniform kernel operations

The kernel should expose a small uniform object-ABI surface rather than one bespoke syscall per object family.

Initial conceptual operations:

- `object_query_interface`
- `object_get_property`
- `object_set_property`
- `object_call`
- `object_subscribe`
- `object_unsubscribe`
- `object_wait` or `object_poll`
- `handle_duplicate`
- `handle_close`

Possible later operations:

- `object_list_children`
- `object_open_child`
- `object_resolve_path`
- `object_transfer`
- `object_publish`

The syscall ABI details may differ, but the semantic surface should remain small and uniform.

## 9. Property model

### 9.1 Baseline rule

Properties are accessed through uniform operations, not ad hoc object-specific syscalls.

Conceptual baseline:

- get property by object handle + property ID
- set property by object handle + property ID

The first contract should use caller-provided buffers rather than specializing around register returns.

### 9.2 Property kinds

Properties may be:

- read-only
- writable
- array/collection-like
- computed
- accessor-backed
- cached or uncached remote views

### 9.3 Direct mapping

Direct memory mapping of properties may exist later as an optimization for a restricted subset of simple property classes.

It must **not** be the baseline ABI.

Reasons:

- permission checks are harder
- atomicity is harder
- remote/process-backed semantics become inconsistent
- computed/accessor-backed properties do not fit naturally

### 9.4 Missing design questions

Still to define:

- property value encoding
- large-value transport
- atomicity guarantees
- array/append/remove semantics
- caching semantics for remote properties
- change-notification coupling between properties and events

## 10. Method model

### 10.1 Baseline rule

Methods are invoked through a uniform call operation.

The first contract should pass arguments and returns through structured caller-provided buffers.

### 10.2 Method metadata

Methods should describe:

- method ID
- argument schema
- return schema
- required rights
- blocking behavior
- whether async invocation is supported
- exception/failure schema where available

### 10.3 Sync vs async

The kernel contract should distinguish synchronous and asynchronous invocation explicitly.

Synchronous calls:

- run to completion before returning
- may raise structured failures/exceptions

Asynchronous calls:

- return a `JobObject` or equivalent handle
- complete through polling/waiting and/or events

The exact first async surface is still open.

## 11. Event model

### 11.1 Baseline rule

Events are part of the object interface model.

Objects may expose structured events with typed payloads.

### 11.2 Subscription model

The kernel ABI should not treat raw caller function pointers as the cross-process subscription primitive.

Instead, the baseline model should use:

- object handle
- event ID
- delivery target (queue/port/session or equivalent)
- returned subscription handle

This keeps the kernel ABI uniform across:

- kernel-native objects
- hosted objects
- future service objects

Language runtimes may map queue-based delivery into callback syntax.

### 11.3 Event operations

Initial conceptual event operations:

- subscribe to event
- unsubscribe via subscription handle
- wait/poll for delivered events
- inspect event schema and metadata

### 11.4 Event delivery questions still open

- event queue model
- ordering guarantees
- overflow/backpressure behavior
- filtering model
- sync dispatch vs queued dispatch
- reentrancy rules
- permission checks for subscriptions

## 12. Failure and exception transport

### 12.1 Baseline rule

The native object model must support structured failure transport through the same general kernel interface used for objects.

### 12.2 Distinguish these failure classes

The contract should distinguish:

- transport / ABI failure
- handle/rights failure
- object method/property failure
- async completion failure
- subscription/delivery failure

### 12.3 Hosted exceptions

For hosted objects, exceptions do not literally travel across process stacks.

Instead:

- callee failure is captured and serialized
- kernel/runtime transports it through the uniform interface
- caller runtime reconstructs or re-raises it in caller context

### 12.4 Exception object shape

The structured failure model should eventually include at least:

- type/category
- message
- source object/program
- optional code/status
- optional nested cause
- optional structured details payload

## 13. Lifetime and ownership

### 13.1 Handle lifetime

Handles are reference counted.

Default rules:

- creating or duplicating a handle increases a reference count
- closing a handle decreases it
- final release destroys the reference
- if the object is owned solely through those references, it may be destroyed

### 13.2 Hosted child rule

Hosted child objects created by a process should, by default, die when the owning process dies unless ownership has been explicitly transferred or the object has been explicitly published as a shared service.

### 13.3 Multiple references

Multiple references to the same object are allowed.

### 13.4 Subscription lifetime

Subscription objects participate in the same general lifetime model.

Still to define:

- whether subscriptions hold strong or weak references to emitters/targets
- cycle handling strategy
- cleanup semantics when either emitter or subscriber dies

## 14. Namespace and lookup implications

The native object contract should support multiple lookup styles without forcing them into one flat namespace.

Likely categories:

- path-based lookup
- service lookup
- device/platform-tree lookup
- child-object traversal

Potential conceptual operations:

- `open(path)`
- `load(image_path)`
- `connect(service_name)`
- `resolve(parent_handle, child_name_or_id)`

Exact namespace model remains open and should align with VFS and device-tree design.

## 15. Kernel-native OS object

A process should be able to request, receive, and decode the interface of the kernel / OS itself.

The `os` object used in examples is the operating-system object.

The kernel/OS object should be accessible through the same general object contract as other objects, subject to permissions.

This implies the OS object can expose:

- properties
- methods
- events
- interface discovery
- observability and control surfaces

The shell/runtime may provide `os` as a built-in binding to this kernel-native object.

## 16. POSIX compatibility implications

`os1` aims to preserve POSIX compatibility where useful, but the kernel-native contract should not be forced into a pure file-descriptor mindset internally.

The likely direction is:

- native object/handle model internally
- POSIX compatibility layered over it where practical
- FD table as a compatibility veneer over native handles or a closely related primitive

This document does not freeze the POSIX mapping, but the kernel contract should be designed so that mapping remains feasible.

## 17. First implementation slice

The first implementation should stay narrow.

Recommended first slice:

- handle table
- rights bits
- interface query
- property get
- property set
- synchronous method call
- event subscription via queue-style delivery
- handle duplicate / close

Recommended deferrals:

- direct property mapping
- shared service publication
- async jobs beyond what is already needed for process lifecycle
- transfer semantics beyond basic design placeholders
- broad language/runtime sugar

## 18. Major open questions

These need explicit decisions in follow-on revisions:

- exact syscall ABI shape for object operations
- handle-table layout and inheritance rules
- exact rights bit layout
- value encoding format for arguments/results/properties/events
- queue/port model for event delivery
- sync vs async method-call surface
- service publication and multi-client sharing model
- property caching and coherency rules
- how POSIX FDs map onto native handles
- how much of the interface metadata is visible at runtime vs compile time

## 19. Short thesis

The `os1` native object kernel contract should provide a uniform, handle-based kernel substrate for discovering and interacting with heterogeneous objects through properties, methods, events, and structured failures, while remaining honest about isolation boundaries and suitable as the foundation for both the native shell language and a later POSIX compatibility layer.

