# Unified OS API Architecture Review - 2026-05-10

Status: architectural review and canonicalization proposal.

Scope: all files in `doc/os-api-draft/`:

- `README.md`
- `native_object_kernel_contract.md`
- `object_oriented_vfs_spec.md`
- `elf_interface_spec.md`
- `os1-shell-language-first-draft.md`

Cross-checks: `GOALS.md`, `doc/ARCHITECTURE.md`, and `doc/2026-05-10-review.md` were used only to verify established project principles and current implementation constraints. The review treats the draft set as design intent, not implemented ABI.

## Executive Summary

The draft set has a strong coherent center: OS resources are objects; authority flows through rights-bearing handles; interfaces are discoverable as properties, methods, and events; POSIX is a compatibility projection; ELF metadata is a first transport for introspectable program objects; the shell should expose those ideas without hiding isolation boundaries.

The main problem is that the documents currently specify the same core abstractions at different layers without one canonical contract. The largest blockers are the descriptor/handle ABI, property mutability, event delivery, namespace traversal, rights/capability semantics, object lifetime, typed value serialization, and the ELF loader/runtime boundary. These are not minor wording issues. They decide the shape of VFS, sockets, PTYs, filesystem-backed `exec`, services, observability, and the native shell.

Preferred direction:

- Use one per-process descriptor table at the syscall edge. Entries are fd-shaped integers for compatibility, but each entry is a native object handle with rights and optional per-open state.
- Keep the kernel semantic model to objects, handles, interface descriptors, typed values, event queues, and rights. Keep language callbacks, property sugar, and high-level class interpretation in userspace runtimes.
- Make interface descriptors canonical and transport-neutral. ELF `.os.interface` is a source format, not the runtime contract itself.
- Let VFS resolve paths to handles. Canonical child traversal should be through a `children: Collection<ObjectRef<T>>` property, with direct `lookup/list` methods allowed only as early bootstrap shims.
- Standardize property semantics as `get_property` plus optional setter-backed `set_property`. Complex state transitions should remain explicit methods.
- Make event delivery queue/port based in the kernel. Callback syntax in the shell is runtime sugar over subscription handles and queues.
- Define credentials, capabilities, and per-handle rights before expanding observability and object control APIs.
- Write the handle ABI, value ABI, descriptor schema, event queue model, lifecycle model, and namespace rules before implementation moves beyond the current minimal syscall set.

## Critical Blockers

### C1. Descriptor, Handle, And POSIX FD Contract Is Not Canonical

References:

- `README.md:34-42`: the next decision is the "minimal per-process descriptor or handle contract" and should preserve "fd-shaped user APIs at the syscall edge".
- `native_object_kernel_contract.md:117-128`: a handle is a "process-owned reference" and the ABI "should be defined in terms of handles, not raw pointers".
- `object_oriented_vfs_spec.md:50-63`: a handle contains or implies the object, rights, owning process, per-open state, cursor/stream position, and subscription state.
- `object_oriented_vfs_spec.md:1042-1059`: maps POSIX `file descriptor` directly to `object handle`.
- `GOALS.md:481-483` and `doc/2026-05-10-review.md:103-109`: the handle/descriptor decision gates VFS, sockets, PTYs, file-backed `exec`, multiuser identity, and SSH.

Problem:

The drafts use `descriptor`, `handle`, `fd`, and `object handle` as overlapping concepts. The README wants fd-shaped APIs at the syscall edge, the kernel contract wants handles, and the VFS spec maps POSIX file descriptors to handles without defining whether POSIX fd numbers and native handle IDs are the same table, different tables, or projections over one table.

Implementation risk:

If VFS or sockets are implemented before this is settled, the kernel may grow parallel resource tables: POSIX fd tables, object handle tables, event subscription tables, and process/job tables. That creates duplicated lifetime rules, inconsistent close/dup/exec inheritance behavior, and a hard-to-fix POSIX compatibility layer.

Resolution options:

1. POSIX fd table only.
   - Trade-off: fastest compatibility path, but violates the native object principle and forces devices, services, events, and process objects into file-like behavior.
2. Native object handle table only, not fd-shaped.
   - Trade-off: clean native model, but breaks the explicit compatibility goal and makes libc/POSIX projection harder than necessary.
3. Single descriptor table with fd-shaped integer indices and native object entries.
   - Trade-off: one lifetime/inheritance path and POSIX-friendly numbers, but descriptor entries must carry enough type, rights, and per-open state to avoid collapsing into POSIX semantics.
4. Separate fd and native handle tables with explicit bridge objects.
   - Trade-off: clear layering, but expensive and likely redundant for early VFS, sockets, PTYs, and event queues.

Preferred:

Option 3. Define a per-process descriptor table now. A descriptor is an integer index naming a handle table entry. Each entry references a kernel object, granted rights, descriptor flags, per-open state, optional cursor state, close-on-exec/inherit flags, and type metadata. POSIX fd APIs use the same indices; native syscalls interpret the entry as an object handle.

Open design questions:

- Are descriptor numbers recycled immediately or generation-tagged?
- Which flags are inherited across `spawn`/`exec` by default?
- Does `dup` duplicate per-open state or create a new open state object?
- Are event queues, subscriptions, process objects, and timers all descriptors?

### C2. Property Mutability Contradicts Itself

References:

- `native_object_kernel_contract.md:246-253`: baseline property access includes get and set by object handle plus property ID.
- `native_object_kernel_contract.md:255-264`: properties may be read-only, writable, collection-like, computed, accessor-backed, cached, or uncached.
- `object_oriented_vfs_spec.md:767-771`: "Properties are externally read-only. Objects are changed by calling methods, not by assigning property values directly."
- `elf_interface_spec.md:7`: properties may be "read-only / read-write data".
- `elf_interface_spec.md:182-190`: property fields include `access (ro | rw)` and `symbol OR (get_symbol / set_symbol)`.
- `os1-shell-language-first-draft.md:338-349`: property kinds include writable and remote-backed.
- `os1-shell-language-first-draft.md:352-359`: examples include `ls_arg.input.mask += "*.jpg"`.

Problem:

The kernel and ELF drafts allow writable properties and a uniform `set_property` operation. The VFS draft says properties are externally read-only and mutation must happen through methods. The shell draft exposes assignment-like property mutation. These are mutually incompatible unless the term `property` is split into value property, property binding, and object-valued property.

Implementation risk:

The kernel ABI, descriptor schema, language runtime, and VFS providers will disagree about whether a write is `set_property`, `call_method`, or mutation of an object returned by a property. That affects permissions, atomicity, auditing, caching, generated bindings, and POSIX `stat`/`chmod`/`truncate` projection.

Resolution options:

1. Make all properties read-only and require all mutation through methods.
   - Trade-off: simple security model and explicit transitions, but poor ergonomics and contradicts current kernel, ELF, and shell drafts.
2. Allow direct writable scalar properties through `set_property`; require complex resource mutations through methods.
   - Trade-off: preserves uniform property access and shell ergonomics, but requires property descriptors to define atomicity, validation, caching, and event coupling.
3. Treat property assignment purely as language sugar for setter methods.
   - Trade-off: keeps method-only kernel semantics, but makes introspection less direct unless setters are linked canonically to properties.
4. Allow direct memory-backed mutable properties.
   - Trade-off: fastest for local in-process objects, but conflicts with isolation, rights checks, remote objects, and the kernel contract's warning against direct mapping as baseline.

Preferred:

Option 2, with an explicit rule: a property binding is externally visible through `get_property` and may optionally support setter-backed `set_property`. The descriptor must declare access mode, setter semantics, atomicity, cacheability, change event, and required rights. Object-valued properties return handles; mutating the returned object happens through that object's methods. Direct memory-backed writable properties are excluded from v1 except as explicit `MemoryStream`/mapping objects.

Open design questions:

- Is `+=` on a remote property required to be read-modify-write atomic?
- Does `set_property` emit a standardized `property_changed` event?
- Are setters cancellable or blocking method calls under the hood?
- How are partial writes to large properties represented?

### C3. Event Delivery Model Conflicts Across Kernel, ELF, And Shell

References:

- `native_object_kernel_contract.md:336-343`: the ABI should not use raw function pointers; use object handle, event ID, delivery target, and returned subscription handle.
- `native_object_kernel_contract.md:351`: language runtimes may map queue-based delivery into callback syntax.
- `native_object_kernel_contract.md:362-370`: queue model, ordering, overflow, filtering, sync dispatch, and reentrancy are still open.
- `elf_interface_spec.md:255-260`: event operations are `add(callback)`, `remove(handle)`, `list()`, and internal `emit`.
- `elf_interface_spec.md:264-269`: runtime model is `obj.events.changed.add(fn)`.
- `os1-shell-language-first-draft.md:394-405`: `+=` registers a handler and returns a subscription object handle.
- `os1-shell-language-first-draft.md:428-435`: events emitted during a synchronous hosted call are delivered synchronously and handler exceptions bubble.
- `os1-shell-language-first-draft.md:444-451`: reentrant calls during active dispatch are dangerous and may be rejected.

Problem:

The kernel contract correctly rejects raw cross-process callbacks. The ELF draft describes callback registration as if it were the event ABI. The shell draft requires synchronous delivery during hosted calls, which implies cross-process reentrancy and exception propagation through event dispatch unless implemented very carefully as runtime sugar.

Implementation risk:

Raw callbacks or synchronous cross-process dispatch can deadlock the caller and hosted process, violate isolation, break stack ownership, and make exception propagation ambiguous. It also makes event ordering, queue overflow, cancellation, and subscription cleanup impossible to reason about consistently.

Resolution options:

1. Kernel-level callbacks.
   - Trade-off: simple syntax, but violates the established design principle and is unsafe across processes.
2. Pure queue/port delivery; callbacks are userspace runtime sugar.
   - Trade-off: coherent ABI and isolation, but the shell runtime needs an event loop or dispatch point.
3. Hybrid: synchronous call-scoped event queues plus normal async queues.
   - Trade-off: supports progress events during blocking calls, but adds complexity and must define backpressure and handler failure semantics.
4. No events in v1; only polling and waitable state.
   - Trade-off: simpler first implementation, but delays a core design goal and weakens observability.

Preferred:

Option 2 for the kernel ABI, with a narrow form of option 3 only if needed. `object_subscribe` takes an emitter handle, event ID, filter/options, and an event queue descriptor. It returns a subscription descriptor. The kernel delivers typed event records to queues. Shell `+= callback` installs runtime dispatch on a queue. Synchronous hosted calls may optionally return progress through a call-scoped queue, but must not invoke caller code on the hosted object's stack or kernel stack.

Open design questions:

- Are event queues descriptors or object-valued properties under `/os/events`?
- Are queues per-process, per-thread, or explicit descriptors?
- What is the default overflow policy: drop newest, drop oldest, coalesce, backpressure, or fail emitter?
- Can handler exceptions cancel the subscription, fail the call, or only fail the runtime dispatch task?

### C4. VFS Namespace Traversal Has Duplicated Responsibilities

References:

- `native_object_kernel_contract.md:442-458`: multiple lookup styles are allowed: path lookup, service lookup, device-tree lookup, and child traversal.
- `object_oriented_vfs_spec.md:7`: VFS is a kernel-managed hierarchical namespace resolving paths to object handles.
- `object_oriented_vfs_spec.md:85-89`: child objects are `Collection<T>` properties.
- `object_oriented_vfs_spec.md:142-151`: conceptual operations include `open(path, requested_rights)`.
- `object_oriented_vfs_spec.md:153-162`: child traversal is not special; `directory.children.lookup("name")`.
- `object_oriented_vfs_spec.md:730-740`: directory objects also expose direct `list`, `lookup`, `create_file`, `create_directory`, `unlink`, `rename`, and `mount`.
- `object_oriented_vfs_spec.md:881-897`: `Collection<T>` defines `list`, `get`, and `contains`, but not `lookup`.
- `object_oriented_vfs_spec.md:999-1038`: providers implement both `open(path)` and object operations.

Problem:

Path resolution, directory methods, `children` collections, provider `open(path)`, and collection `get/list` all own overlapping parts of traversal. The docs also use `lookup` in examples while the standard collection type defines `get`. It is unclear whether a directory is itself the authoritative child lookup object, whether `children` is, or whether providers bypass both.

Implementation risk:

The VFS may grow multiple lookup paths with inconsistent permission checks, cache rules, mount traversal, rename/unlink behavior, and path race semantics. Filesystem providers may implement path parsing privately instead of participating in a shared namespace contract.

Resolution options:

1. Directory methods are canonical; `children` is just introspection.
   - Trade-off: close to traditional VFS, but weakens the "children as collection" model.
2. `children: Collection<ObjectRef<T>>` is canonical; directory methods are bootstrap compatibility shims.
   - Trade-off: aligns with the interface model, but requires a solid collection spec before VFS feels natural.
3. Provider `open(path)` is canonical; object traversal is optional high-level sugar.
   - Trade-off: easiest to implement initially, but duplicates path logic across providers and weakens object uniformity.
4. Split namespace lookup and collection enumeration explicitly.
   - Trade-off: clean for paths and collections, but introduces an extra `Namespace` standard type.

Preferred:

Option 2 with a small part of option 4. Define a canonical `Namespace` or `Collection<ObjectRef>` standard interface with `lookup(name)`, `list(options)`, `watch(options)`, and optional mutation methods. Directory objects expose `children` with that interface. Kernel VFS `open(path)` walks namespace objects using that same interface internally. Direct directory `lookup/list` methods may exist in the bootstrap milestone but should be specified as aliases.

Open design questions:

- Does path lookup return the target object handle or a handle with per-open state?
- How are mount points represented: child entries, provider bindings, object links, or namespace overlays?
- Are lookup names byte strings, UTF-8 strings, interned IDs, or provider-defined components?
- How are path races handled across `rename`, `unlink`, and object lifetime?

### C5. Ambient `os` Object Duplicates The `/os` Namespace

References:

- `native_object_kernel_contract.md:460-476`: the `os` object is a kernel-native object instance exposed through the same object contract.
- `object_oriented_vfs_spec.md:218-295`: `/os/system` and `/os/kernel` expose system and kernel state.
- `object_oriented_vfs_spec.md:299-673`: `/os/processes`, `/os/devices`, `/os/drivers`, `/os/services`, `/os/memory`, and `/os/events` expose runtime branches.
- `os1-shell-language-first-draft.md:560-575`: `os` is the ambient system object and bridge to process control, filesystems, users, devices, networking, timing, observability, and system control.
- `os1-shell-language-first-draft.md:597-884`: `os.process`, `os.user`, `os.env`, `os.fs`, `os.net`, `os.devices`, `os.time`, `os.observe`, and `os.sys` are top-level properties.
- `os1-shell-language-first-draft.md:896-980`: `os.load`, `os.spawn`, `os.open`, `os.find`, `os.connect`, `os.observe`, `os.sleep`, `os.require`, and `os.import` are core methods.

Problem:

The same capabilities appear as VFS objects, branch methods, and ambient `os` object properties/methods. For example, process state appears in `/os/processes`, `os.process`, `os.spawn`, and process methods; observability appears in `/os/events`, `os.observe` as a property, and `os.observe(...)` as a method.

Implementation risk:

The project may implement two OS APIs: a native VFS object model and a shell-specific `os` object model. That would create different permission checks, different object identities, duplicate method names, and ambiguous documentation for the same functionality.

Resolution options:

1. Make `/os` canonical and remove most ambient `os` convenience APIs.
   - Trade-off: maximal uniformity, but less ergonomic for shell use.
2. Make the ambient `os` object canonical and treat `/os` as a projection.
   - Trade-off: shell-friendly, but couples kernel design to language ergonomics.
3. Define `os` as a capability-limited process context object whose properties are handles/projections into canonical `/os` objects.
   - Trade-off: preserves ergonomics while keeping one source of truth, but requires clear projection rules.
4. Keep both as independent APIs.
   - Trade-off: short-term flexibility, but long-term duplication.

Preferred:

Option 3. The canonical OS object model lives in object handles and the native namespace. The shell's `os` binding is a context object supplied by the runtime. It exposes convenience properties and methods, but each maps to canonical objects or operations. For example, `os.process` is the current process handle, `os.fs.open` is a convenience wrapper over VFS `open`, and `os.sys.shutdown` maps to `/os/system.shutdown` with the current credentials.

Open design questions:

- Is `/os/system` the same object identity as ambient `os.sys`, or a projected child?
- Should `os.open` and `os.fs.open` both exist?
- Should `os.observe` be renamed so it is not both a property and method?
- How does a restricted sandbox receive a reduced `os` object?

### C6. ELF Metadata Is Over-Coupled To Symbols And Raw Calling Conventions

References:

- `native_object_kernel_contract.md:67-73`: avoid assuming direct property mapping, raw callback pointers, tight ELF coupling, and POSIX fd internals.
- `native_object_kernel_contract.md:189`: the contract must not assume ELF forever.
- `elf_interface_spec.md:105-124`: runtime addresses are computed from symbol values and `.symtab`/`.dynsym`.
- `elf_interface_spec.md:142-149`: direct memory properties read/write memory directly.
- `elf_interface_spec.md:198-217`: methods reference symbols and a raw calling convention.
- `elf_interface_spec.md:223-230`: raw C ABI should not be relied on indefinitely.
- `os1-shell-language-first-draft.md:123-132`: `os.load` creates hosted child objects backed by protected child processes, and method calls cross process boundaries.

Problem:

The ELF draft is useful as metadata transport, but it currently describes direct symbol resolution and function invocation as if runtime binding can call addresses directly. That is compatible with in-process loads, but not with hosted process isolation. It also conflicts with the kernel contract's desire to remain transport-neutral and avoid direct memory mapping as baseline.

Implementation risk:

The loader could accidentally become an in-kernel dynamic linker and RPC stub generator that calls user addresses, or the shell could assume a function-pointer ABI that cannot work across process boundaries. This would be hard to secure, hard to version, and incompatible with non-ELF object/package formats.

Resolution options:

1. ELF metadata maps directly to callable symbols for both in-process and hosted objects.
   - Trade-off: simple for prototypes, but wrong for process isolation and kernel security.
2. ELF is only a metadata transport; the runtime converts it to a canonical interface descriptor.
   - Trade-off: clean separation, but requires a descriptor schema and binding/runtime layer.
3. Define separate metadata profiles for in-process libraries and hosted services.
   - Trade-off: explicit semantics, but risks divergence unless both lower to one descriptor model.
4. Defer ELF interface metadata until VFS and descriptors exist.
   - Trade-off: avoids premature ABI, but delays introspectable objects.

Preferred:

Option 2 with profile tags from option 3. `.os.interface` should describe object interfaces in a transport format. The loader/runtime resolves symbols only inside the process that owns the image. Hosted calls cross process boundaries through IPC stubs generated or registered by the hosted runtime. The kernel sees canonical descriptors, typed value envelopes, handles, and event queues, not raw function pointers or raw data symbols.

Open design questions:

- Does the kernel parse `.os.interface`, or does a trusted loader service parse and register descriptors?
- What is the minimum hosted-object entrypoint/constructor ABI?
- How are symbol names stripped or hidden while preserving interface binding?
- Are PIE/shared-object and `ET_EXEC` profiles both supported in v1?

### C7. Rights, Capabilities, Credentials, And Permissions Are Divergent

References:

- `native_object_kernel_contract.md:191-214`: conceptual rights include `inspect`, `read_property`, `write_property`, `call`, `subscribe`, `observe`, `control`, `duplicate`, and `transfer`.
- `object_oriented_vfs_spec.md:941-972`: suggested rights include `RIGHT_DESCRIBE`, `RIGHT_GET_PROPERTY`, `RIGHT_CALL_METHOD`, `RIGHT_SUBSCRIBE_EVENT`, collection mutation rights, `RIGHT_ADMIN`, and `RIGHT_DELEGATE`.
- `elf_interface_spec.md:51-58`: metadata includes permissions.
- `elf_interface_spec.md:378-385`: require capabilities/handles and enforce permissions at interface level.
- `os1-shell-language-first-draft.md:1136-1166`: security concepts include current user identity, capabilities, per-object rights, namespace permissions, event subscription permissions, and observability permissions.
- `object_oriented_vfs_spec.md:687-694` and `object_oriented_vfs_spec.md:721-728`: files/directories have owners and `Permissions`.
- `README.md:11-20`: credentials and permission checks are not implemented.

Problem:

The drafts mix handle rights, capabilities, user/group credentials, file permissions, namespace permissions, interface-item permissions, delegation rights, and string-based `os.user.can("observe", target)` checks. None is canonical, and the relationship between open-time policy and per-operation enforcement is undefined.

Implementation risk:

Security checks will be ad hoc. Observability, kernel symbol lookup, device control, process control, and file permissions may each implement separate policy paths. Later multiuser support would need retrofits across every API.

Resolution options:

1. Pure ACL/user/group model.
   - Trade-off: familiar for files, but weak for object handles, delegation, and IPC.
2. Pure capability/handle model.
   - Trade-off: strong authority model, but needs a bootstrap identity policy and POSIX projection.
3. Hybrid: credentials and policy grant attenuated handle rights at open/connect/load time; operations enforce handle rights.
   - Trade-off: more machinery, but fits objects, files, services, and POSIX.
4. Defer security until after VFS works.
   - Trade-off: faster prototype, but conflicts with current review warnings about observability growth and will create retrofits.

Preferred:

Option 3. Define credentials as process state. Define policy modules for namespace lookup and object-specific authorization. Successful open/connect/load creates a descriptor with an attenuated rights mask plus optional per-interface-item grants. All subsequent operations check descriptor rights first. File owner/permissions are policy inputs, not the universal security model. Capabilities are transferable/delegatable authorities represented as rights-bearing handles or credential claims.

Open design questions:

- What are the base rights bits in v1?
- Are per-method rights encoded as bitsets, descriptor masks, capability tokens, or descriptor table entries?
- Which observability data is world-readable, same-user-readable, privileged, or kernel-only?
- How are rights attenuated on descriptor transfer or inheritance?

### C8. Object Lifetime And Ownership Rules Are Incomplete

References:

- `native_object_kernel_contract.md:411-420`: handle creation/duplication increments references; close decrements; final release may destroy solely referenced objects.
- `native_object_kernel_contract.md:422-424`: hosted child objects die when the owning process dies unless transferred or published.
- `native_object_kernel_contract.md:430-438`: subscription strong/weak references, cycle handling, and cleanup are still undefined.
- `object_oriented_vfs_spec.md:976-995`: objects may be persistent path-backed, runtime path-backed, handle-only, ephemeral, services, or shared memory.
- `os1-shell-language-first-draft.md:250-263`: final release destroys the hosted object and parent script death destroys child hosted objects.
- `os1-shell-language-first-draft.md:277-287`: explicit `close`, `dispose`, transfer, and detach are future possibilities.
- `object_oriented_vfs_spec.md:534-589`: services can be started, stopped, restarted, and open client sessions.

Problem:

The drafts combine reference-counted handles, process ownership trees, path-backed persistence, service publication, shared memory, event subscriptions, and runtime services without a single lifecycle model. "Final release destroys the hosted object" and "parent death destroys child hosted objects" are not enough for shared services, published objects, persistent files, event queues, or shared memory.

Implementation risk:

Objects may leak, be destroyed while still published, keep processes alive accidentally through cycles, or be killed when a client exits. Event subscriptions are especially dangerous because emitter, queue, subscriber, and handler references can form cycles.

Resolution options:

1. Pure reference counting for all live objects.
   - Trade-off: simple, but cycles and service roots need special cases.
2. Process ownership tree dominates; handles are secondary.
   - Trade-off: clear cleanup on process death, but bad for shared services and named objects.
3. Rooted object graph: handles, namespaces, services, and kernel subsystems are roots with explicit strong/weak edge types.
   - Trade-off: more formal, but covers files, services, queues, subscriptions, and hosted children.
4. Garbage collection across kernel objects.
   - Trade-off: handles cycles, but too heavy and surprising for kernel resource management.

Preferred:

Option 3. Define object lifetime by root sets and reference edges. Descriptor entries hold strong references. Namespaces may hold persistent or runtime roots. Services are rooted by a service manager while registered. Hosted children are rooted by the owning process unless detached or published. Subscriptions should hold a strong reference to the subscription and queue, but weak references to emitter/target unless the subscription options explicitly request a keepalive. Closing a descriptor must be deterministic.

Open design questions:

- What exact operation publishes or detaches a hosted object?
- Can a service survive its backing process exit, or is a service object a supervisor-managed state machine?
- Are shared memory objects destroyed when all mappings close, all descriptors close, or both?
- What is the cleanup order when emitter, queue, or subscriber exits?

### C9. Type System, Serialization, And Standard Library Have A Bootstrap Cycle

References:

- `native_object_kernel_contract.md:279-288`: property encoding, large-value transport, atomicity, collection semantics, caching, and notification coupling are not defined.
- `native_object_kernel_contract.md:517-526`: exact syscall ABI, value encoding, event queues, POSIX FD mapping, and runtime metadata visibility are open.
- `object_oriented_vfs_spec.md:97-132`: standard class libraries define primitive/common types, but the kernel only needs enough type information to validate access, route calls, preserve ABI compatibility, and enforce rights.
- `object_oriented_vfs_spec.md:839-918`: streams, collections, maps, dictionaries, memory streams, and arrays are standard library types.
- `object_oriented_vfs_spec.md:1174-1185`: descriptor encoding, ID stability, kernel type validation, object-reference serialization, and class-library ABI boundaries are open.
- `elf_interface_spec.md:272-321`: encoding options include JSON/YAML, binary schema, or ELF notes.
- `os1-shell-language-first-draft.md:170-210`: type discovery and type query syntax depend on metadata.

Problem:

The VFS depends on `Stream<T>`, `Collection<T>`, `ObjectRef<T>`, and structured value types before the value ABI, type IDs, descriptor schema, and class-library boundary exist. The kernel is expected to validate and route typed calls, but the type system is not specified. This is a circular dependency: VFS needs standard types; standard types need descriptor and serialization rules; descriptor and serialization rules are deferred.

Implementation risk:

Every subsystem may invent its own structs, IDs, string type names, and marshaling rules. That breaks introspection, generated bindings, IPC, POSIX projection, and stable ABI evolution.

Resolution options:

1. Text schemas only in v1.
   - Trade-off: debuggable and easy to generate, but expensive to parse in the kernel and hard to make ABI-stable.
2. Compact binary descriptor/value ABI only.
   - Trade-off: stable and efficient, but harder to inspect and evolve.
3. Binary canonical ABI plus text source/diagnostic representation.
   - Trade-off: more work up front, but best long-term coherence.
4. Opaque byte buffers with user-space-only schemas.
   - Trade-off: fast to implement, but kernel cannot enforce rights or route typed object references safely.

Preferred:

Option 3. Define a minimal canonical type/value ABI before full VFS: primitive IDs, arrays, records, enums, optional/result, strings/bytes, handles/object references, and versioned descriptors. Text JSON/YAML can be a source or debugging format for ELF metadata, but runtime syscall and IPC transport should use a compact typed envelope.

Open design questions:

- Which types are kernel ABI versus OS runtime library ABI?
- Are type IDs globally assigned, interface-local, package-local, or content-hashed?
- How are handles serialized across IPC: transferred descriptor, borrowed reference, or object capability token?
- What is the maximum inline value size before out-of-line buffers or shared memory are required?

### C10. Kernel, Provider, Loader, Runtime, And Userspace Responsibilities Are Blurred

References:

- `native_object_kernel_contract.md:173-189`: metadata may be parsed and exposed through the kernel, but the kernel contract must support heterogeneous interfaces and not assume ELF forever.
- `object_oriented_vfs_spec.md:22-46`: an object is a kernel-known entity.
- `object_oriented_vfs_spec.md:126-132`: class libraries can live in kernel ABI, OS runtime libraries, or extension components, and the kernel need not fully understand every high-level class.
- `object_oriented_vfs_spec.md:999-1028`: a VFS branch is backed by a provider implementing open, describe, get_property, call_method, and subscribe_event.
- `os1-shell-language-first-draft.md:83-90`: hosted objects live in protected child processes and expose properties, methods, and events.
- `elf_interface_spec.md:353-360`: runtime flow loads ELF, parses metadata, resolves symbols, computes addresses, and builds a structured object.

Problem:

The docs do not decide which component owns descriptor parsing, type validation, provider dispatch, hosted IPC, object registration, class-library interpretation, or ELF binding. "Kernel-known" can mean kernel-owned object, kernel-routed proxy to a userspace service, or userspace runtime object.

Implementation risk:

The kernel may take on too much policy and parsing, or userspace providers may bypass kernel rights enforcement. Hosted objects may become unstructured IPC endpoints rather than first-class object handles.

Resolution options:

1. Kernel owns all object metadata, validation, and dispatch.
   - Trade-off: strong enforcement, but too much kernel complexity and too much ELF/type parsing in privileged code.
2. Userspace owns metadata and dispatch; kernel only transports bytes.
   - Trade-off: small kernel, but weak uniform rights and introspection.
3. Split responsibilities: kernel owns handles, rights, descriptor registry, namespace, and transport; trusted loaders/providers register canonical descriptors; userspace hosts implement method bodies.
   - Trade-off: balanced, but requires registration and provider contracts.
4. Defer hosted objects until kernel-native objects and files work.
   - Trade-off: reduces first implementation risk, but leaves shell/ELF drafts speculative.

Preferred:

Option 3. The kernel should own the object table, descriptor table, namespace routing, rights checks, wait/event queues, and canonical descriptor registry. A trusted loader/runtime service parses ELF metadata and registers descriptors or hosted endpoints. Providers implement object methods behind kernel-routed handles. The kernel validates the envelope enough to enforce object references, rights, sizes, and ABI version, but richer type interpretation can live in userspace libraries.

Open design questions:

- Is the loader service trusted kernel code, a privileged userspace service, or both by phase?
- Can untrusted processes register object providers?
- How does the kernel revoke or invalidate descriptors from a crashed provider?
- What minimum metadata must the kernel understand for safe routing?

### C11. Program Image, Hosted Object, Process, Service, And Job Models Overlap

References:

- `native_object_kernel_contract.md:99-113`: conceptual kinds include `HostedObject`, `InProcessObject`, `ProcessObject`, `ProgramImageObject`, `JobObject`, and `NamespaceObject`.
- `native_object_kernel_contract.md:130-157`: hosted object load and in-process object load are distinct.
- `object_oriented_vfs_spec.md:299-361`: `/os/processes` can spawn processes and expose process objects.
- `object_oriented_vfs_spec.md:534-589`: `/os/services` starts/stops services and opens client sessions.
- `object_oriented_vfs_spec.md:1193-1205`: initial object kinds include `Service`, `Stream`, `Collection`, and `EventSubscription`, but not `HostedObject`, `InProcessObject`, `ProgramImageObject`, or `Job`.
- `os1-shell-language-first-draft.md:235-249`: `os.load(path)` creates a hosted object instance.
- `os1-shell-language-first-draft.md:931-940`: `os.spawn(path, ...)` creates POSIX-compatible one-shot execution and returns a process/job object.

Problem:

The drafts distinguish `load`, `spawn`, service start, hosted object load, in-process load, program image, process, and job, but do not define their relationships. It is unclear whether a hosted object is a process, whether a service is a hosted object, whether a job is a process handle or async operation handle, and whether program images are path-backed file objects or separate objects.

Implementation risk:

The loader, process manager, service manager, shell, and VFS may each create their own lifecycle and handle types. Process supervision, async completion, cancellation, exit status, service restart, and object constructors would diverge.

Resolution options:

1. Collapse everything executable into `ProcessObject`.
   - Trade-off: simple, but loses persistent object semantics and service/session distinction.
2. Treat hosted objects, services, and jobs as independent unrelated types.
   - Trade-off: flexible, but duplicates lifecycle and control APIs.
3. Define an execution taxonomy: `ProgramImage`, `Process`, `HostedObject`, `Service`, `ClientSession`, and `Job` as related objects with explicit transitions.
   - Trade-off: more upfront spec work, but coherent and extensible.
4. Implement only `spawn` first; defer `load` and services.
   - Trade-off: pragmatic early phase, but does not answer native API design.

Preferred:

Option 3. A `ProgramImage` is a loadable object image, usually resolved through VFS. A `Process` is an isolated execution context. A `HostedObject` is an object endpoint hosted by a process or runtime. A `Service` is a hosted object registered under service-manager ownership with restart/session policy. A `ClientSession` is a per-client object returned by service connect/open. A `Job` is a waitable async operation result, including but not limited to process execution.

Open design questions:

- Does `os.load` always create a new process, or can it load in-process with explicit options?
- Is constructor failure a process exit, method exception, or loader exception?
- Does `os.spawn` return both `ProcessObject` and `JobObject`, or one object implementing both interfaces?
- Can a service expose multiple hosted object interfaces from one process?

## Medium-Risk Inconsistencies

### M1. Async Methods And `JobObject` Are Underspecified

References:

- `native_object_kernel_contract.md:310-324`: async calls return a `JobObject` or equivalent handle, but the exact surface is open.
- `object_oriented_vfs_spec.md:804-812`: method descriptors have an `async` flag.
- `os1-shell-language-first-draft.md:311-321`: async methods should be explicit and return a job/task object.
- `os1-shell-language-first-draft.md:523-529`: later language features may include `await` and `select`.

Risk:

Without a `Job` interface, every async subsystem will invent completion, cancellation, progress, and error semantics.

Options:

1. Defer async entirely.
2. Define only process wait jobs.
3. Define a generic `Job<T>` object with state, result, failure, cancel, wait, and events.
4. Use event queues only, no job objects.

Preferred:

Option 3. Define `Job<T>` as a standard object type before async methods. It should expose `state`, `result`, `failure`, `cancel()`, `wait(timeout)`, `progress` events, and completion events. Process execution can implement `Job<ProcessExit>`.

Question:

- Are jobs single-consumer or can multiple handles observe the same completion?

### M2. Failure, Exception, `Result<T>`, And POSIX Errno Models Are Not Unified

References:

- `native_object_kernel_contract.md:378-387`: failure classes include transport, rights, object, async, and subscription/delivery failures.
- `native_object_kernel_contract.md:398-407`: exception shape includes type/category, message, source, optional code/status, nested cause, and details.
- `object_oriented_vfs_spec.md:113-114`: the standard library includes `Result<T>` and `Optional<T>`.
- `object_oriented_vfs_spec.md:903-907`: `MemoryStream.protect` returns `Result<Void>`.
- `os1-shell-language-first-draft.md:457-466`: synchronous operations use exception propagation.
- `os1-shell-language-first-draft.md:488-505`: hosted call failures are serialized and re-raised.

Risk:

Callers may need to handle the same failure as errno, exception, `Result<T>`, event error, or job failure depending on path. POSIX projection and native shell behavior will disagree.

Options:

1. Exceptions only for native API.
2. `Result<T>` only for method returns.
3. Canonical failure envelope, projected as exceptions, `Result<T>`, errno, or job failure by binding.
4. Per-interface failure models.

Preferred:

Option 3. Define a canonical `Failure` envelope and a syscall transport status. Transport/syscall errors are immediate ABI failures. Object-level failures are returned in typed envelopes and projected by the runtime as exceptions or `Result<T>` according to method metadata. POSIX errno is a compatibility mapping from canonical failure codes.

Question:

- Which failures are retryable, cancellable, or security-sensitive?

### M3. Stream Cursor And Per-Open State Are Ambiguous

References:

- `object_oriented_vfs_spec.md:54-61`: a handle may contain optional cursor or stream position.
- `object_oriented_vfs_spec.md:859-872`: `Stream<T>` has `position`, `read`, `write`, `seek`, `flush`, and `close`.
- `object_oriented_vfs_spec.md:1048-1059`: POSIX `read`/`write` map through a file's `data: Stream<byte>`.
- `object_oriented_vfs_spec.md:1219`: file reading/writing becomes method calls on `Stream<byte>`.

Risk:

If a file object's `data` property returns the same `Stream` object to all callers, POSIX fd offsets and independent cursors break. If stream position lives on the descriptor, then object-valued properties need per-open projection rules.

Options:

1. Stream position is global on the stream object.
2. Stream position is per descriptor/open state.
3. `file.data` returns a stream factory or per-open stream view.
4. POSIX read/write bypasses `Stream<T>` and uses file-descriptor-specific syscalls.

Preferred:

Option 2 plus option 3. Stream state should be per open stream descriptor unless explicitly declared shared. Opening a file creates a file descriptor; reading `data` should return a stream-view descriptor tied to that open state, or the file descriptor itself should implement the `Stream<byte>` interface.

Question:

- Is `Stream.close()` equivalent to closing the descriptor that references it?

### M4. Memory Mapping And Shared Memory Lack Ownership, Pinning, And Protection Rules

References:

- `native_object_kernel_contract.md:266-278`: direct property mapping is not baseline because permissions, atomicity, remote semantics, and computed properties are hard.
- `object_oriented_vfs_spec.md:922-937`: memory regions are `Collection<MemoryStream>`, `MemoryStream`, or `MemoryArray`, and access rights are enforced by returned memory objects.
- `object_oriented_vfs_spec.md:603-631`: `/os/memory` can allocate shared memory and report statistics.
- `object_oriented_vfs_spec.md:355-361`: processes can open controlled memory views.
- `GOALS.md:481-483`: descriptor/resource lifetime is a prerequisite for VFS and related subsystems.

Risk:

Memory mapping touches address-space lifetime, revocation, TLB shootdown, debugger rights, shared-memory naming, DMA pinning, and cache policy. Leaving it as `MemoryStream.map` without a memory object contract risks unsafe mappings and unrevokable shared state.

Options:

1. Defer all mapping; stream I/O only.
2. Implement anonymous shared memory only.
3. Define `MemoryObject`/`Mapping` objects with rights, lifetime, protection, and revocation.
4. Treat mapping as a method on any stream.

Preferred:

Option 3, with option 1 for earliest VFS if needed. Define `MemoryObject`, `Mapping`, protection rights, copy-on-write/share modes, ownership, and close/unmap semantics before exposing process memory or file mapping.

Question:

- Can mappings outlive the descriptor used to create them?

### M5. Thread-Safety, Atomicity, Blocking, And Reentrancy Are Gaps

References:

- `native_object_kernel_contract.md:281-288`: property atomicity and caching are open.
- `native_object_kernel_contract.md:300-308`: methods describe blocking behavior and async support.
- `native_object_kernel_contract.md:362-370`: event ordering, overflow, sync dispatch, and reentrancy are open.
- `elf_interface_spec.md:424-435`: interface metadata must describe thread-safety, blocking behavior, and error model.
- `os1-shell-language-first-draft.md:444-451`: reentrant calls are dangerous and may be rejected.

Risk:

SMP, event dispatch, process control, streams, and properties will have undocumented race semantics. Generated bindings cannot know whether a property read is coherent, whether a method can block, or whether callbacks may call back into the emitter.

Options:

1. Leave concurrency semantics implementation-defined.
2. Require all object operations to be serialized by the kernel.
3. Add descriptor flags for atomicity, blocking, reentrancy, thread-safety, and ordering guarantees.
4. Require every object type to write its own concurrency spec.

Preferred:

Option 3. The base descriptor schema should include standard flags and defaults: may-block, async-supported, reentrant-safe, thread-safe, atomic-get, atomic-set, ordered-events, lossy-events, and cache policy. Object-specific specs can add details.

Question:

- Are concurrent method calls to the same handle serialized by default?

### M6. Interface ID, Versioning, And Compatibility Rules Are Missing

References:

- `native_object_kernel_contract.md:179-185`: metadata includes version and compatibility information.
- `native_object_kernel_contract.md:302-308`: methods have IDs, schemas, rights, blocking behavior, async support, and failure schemas.
- `object_oriented_vfs_spec.md:93`: early implementations may use static numeric IDs; later ones may use richer metadata.
- `object_oriented_vfs_spec.md:1174-1176`: ID stability and common minimum interface are open.
- `elf_interface_spec.md:65-67`: metadata example has interface string and numeric version.

Risk:

If property/method/event IDs are not stable, descriptors cannot be cached, generated bindings cannot work, and compatibility across service upgrades becomes fragile.

Options:

1. Dynamic IDs assigned at load time.
2. Globally stable numeric IDs.
3. Interface-local stable IDs plus names and versioning.
4. Name-only dispatch.

Preferred:

Option 3. Interface descriptors should use stable interface IDs, semantic versioning or compatibility versions, and interface-local stable numeric member IDs. Names remain required for tooling and diagnostics.

Question:

- Are removed members tombstoned to preserve IDs?

### M7. Observability And Introspection Need Privilege Tiers

References:

- `object_oriented_vfs_spec.md:281-287`: `/os/kernel` exposes symbol info, dumps, and log-level control.
- `object_oriented_vfs_spec.md:355-361`: process objects can open memory views and list handles.
- `os1-shell-language-first-draft.md:824-855`: observability exposes trace sessions, snapshots, metrics, inspect, and dumps.
- `os1-shell-language-first-draft.md:1136-1166`: observability APIs require explicit security.
- `doc/2026-05-10-review.md:110-117`: the current observe ABI already needs world-readable versus privileged tiers.

Risk:

Introspection can leak kernel addresses, scheduler state, process handles, device topology, and security context. If access tiers are not designed now, later multiuser support must retrofit every observe endpoint.

Options:

1. Hide all introspection from unprivileged code.
2. World-readable basic observability and privileged sensitive details.
3. Same-user, same-session, and admin tiers.
4. Per-object policy only, no global tiers.

Preferred:

Option 2 plus option 3. Define standard visibility tiers: public, same-user, same-session, owner, debug, admin, and kernel-only. Object-specific policy can refine them.

Question:

- Is kernel symbol info ever available outside debug/admin builds?

### M8. Lifecycle State Machines Are Listed But Not Defined

References:

- `object_oriented_vfs_spec.md:347`: process state includes running, sleeping, stopped, zombie, etc.
- `object_oriented_vfs_spec.md:508-512`: driver state includes loaded, active, failed, unloading, etc.
- `object_oriented_vfs_spec.md:572-578`: service state includes starting, running, stopped, failed, etc.
- `object_oriented_vfs_spec.md:407-413`, `523-530`, and `591-599`: events report state changes.

Risk:

Without state machines, methods such as `start`, `stop`, `restart`, `suspend`, `resume`, `bind`, and `unbind` have ambiguous preconditions, idempotency, races, and failure behavior.

Options:

1. Leave states descriptive only.
2. Define formal state machines for core object families.
3. Define only generic state-change event rules.
4. Push state transitions into provider-specific specs.

Preferred:

Option 2 for process, service, driver, device, job, subscription, and stream. These should define states, valid transitions, terminal states, method preconditions, event ordering, and idempotency.

Question:

- Are repeated `stop`/`terminate` calls successful no-ops or failures?

### M9. Service Publication And Connection Workflow Is Incomplete

References:

- `native_object_kernel_contract.md:142-143`: hosted object lifetime can be transferred or published through a service mechanism.
- `native_object_kernel_contract.md:232-239`: possible later operations include `object_transfer` and `object_publish`.
- `object_oriented_vfs_spec.md:534-589`: `/os/services` has start, stop, restart, service objects, and `open_client`.
- `os1-shell-language-first-draft.md:956-959`: `os.connect` connects to an existing service or endpoint.

Risk:

Service discovery, publication, client sessions, restart policy, ownership, and multi-client sharing will be improvised per service.

Options:

1. Use VFS paths only for service discovery.
2. Use a service manager registry only.
3. Define service publication as registering a named object handle into a namespace/registry with policy.
4. Defer services until process and VFS are stable.

Preferred:

Option 3. A service manager should root service objects, publish names into `/os/services`, enforce connect policy, create per-client session objects, and define restart behavior.

Question:

- Does `connect` return the service object itself or a new client session object?

### M10. POSIX Projection Scope Is Too Vague

References:

- `README.md:36-42`: fd-shaped user APIs should be preserved unless deliberately changed.
- `native_object_kernel_contract.md:478-488`: POSIX should be layered over native handles.
- `object_oriented_vfs_spec.md:1042-1061`: maps POSIX `open`, `read`, `write`, `stat`, `opendir`, `ioctl`, `mmap`, and `poll/select/epoll` to native operations.
- `os1-shell-language-first-draft.md:534-552`: POSIX execution and text streams remain available for compatibility.
- `os1-shell-language-first-draft.md:1169-1179`: hosted objects, spawn, files, streams, and descriptors coexist with POSIX.

Risk:

If POSIX is underspecified, early fd-compatible choices may accidentally constrain native semantics, or native handles may become impossible to expose through libc.

Options:

1. Implement native first, POSIX later.
2. Implement POSIX fd semantics first, native later.
3. Define the minimal POSIX projection contract now but implement incrementally.
4. Maintain separate POSIX and native kernelside paths.

Preferred:

Option 3. Specify fd numbering, read/write stream mapping, `close`, `dup`, `fork`/`exec` inheritance policy if applicable, `poll` over event queues, errno mapping, and `mmap` prerequisites. Then implement only the slice needed for console, VFS files, PTYs, sockets, and exec.

Question:

- Is `fork` in scope for compatibility, or only `spawn`/`exec`?

### M11. Shell `Handle` Terminology Confuses Runtime References With Kernel Handles

References:

- `native_object_kernel_contract.md:117-128`: a handle is a process-owned rights-bearing reference used by the kernel ABI.
- `object_oriented_vfs_spec.md:52-63`: handle contains object, rights, owning process, and per-open state.
- `os1-shell-language-first-draft.md:92-103`: handles may refer to local or hosted objects and are governed by language lifetime rules.
- `os1-shell-language-first-draft.md:61-72`: local objects live in the script process address space.

Risk:

If local runtime references are called handles, documentation and APIs may imply that local objects have kernel rights, descriptor table entries, or transferable authority. That blurs security boundaries.

Options:

1. Use `handle` for both local and kernel references.
2. Reserve `handle` for kernel object descriptors; use `reference` for local objects.
3. Use `ObjectRef` for both, with subtypes for local and kernel-backed.

Preferred:

Option 2. The shell should call local references `references` or `local object references`. `Handle` should mean a kernel-recognized rights-bearing descriptor/object reference. Language values may wrap handles, but not all references are handles.

Question:

- Can a local object ever be exported and become a hosted/kernel object?

## Low-Risk Cleanup Items

### L1. Operation Names Are Inconsistent

References:

- `native_object_kernel_contract.md:220-230`: `object_query_interface`, `object_get_property`, `object_call`, `object_wait`.
- `object_oriented_vfs_spec.md:142-151`: `describe`, `get_property`, `call_method`, `wait`.
- `object_oriented_vfs_spec.md:1077-1083`: first milestone operations are `open`, `close`, `info`.
- `object_oriented_vfs_spec.md:1207-1217`: first version uses `describe_basic`.

Risk:

The ABI and docs will drift before implementation starts.

Options:

1. Keep aliases.
2. Pick canonical syscall names and allow user-facing aliases.
3. Delay naming until implementation.

Preferred:

Option 2. Canonical semantic names: `open`, `close`, `dup`, `query_interface`, `get_property`, `set_property`, `call`, `subscribe`, `unsubscribe`, `wait`, `transfer`. Language and POSIX layers can wrap them.

### L2. Type Names Are Not Normalized

References:

- `object_oriented_vfs_spec.md:105-123`: examples use `Bool`, `Int32`, `UInt64`, `Uuid`, `Timestamp`, `Duration`.
- `object_oriented_vfs_spec.md:230-238`: branch specs use `string`, `timestamp`, `u64`, `uuid`.
- `elf_interface_spec.md:69-78`: ELF examples use `u64`.
- `os1-shell-language-first-draft.md:48-58`: shell names categories generically.

Risk:

Generated descriptors and language bindings may treat `String`, `string`, `u64`, and `UInt64` as different ABI types.

Options:

1. Lowercase primitive names.
2. PascalCase standard-library names.
3. Numeric canonical type IDs with preferred display names.

Preferred:

Option 3. Use canonical type IDs in descriptors and define one preferred spelling per language binding. For docs, use `Bool`, `Int32`, `UInt64`, `String`, `Bytes`, `Uuid`, `Timestamp`, and `Duration` consistently, or explicitly choose lowercase C-like names.

### L3. Path Syntax Needs One Rule

References:

- `object_oriented_vfs_spec.md:67-69`: example `/os/devices/gpu0`.
- `object_oriented_vfs_spec.md:212`: canonical native examples include `/os/devices/net[0]` and `/os/devices/gpu[0]`.
- `object_oriented_vfs_spec.md:305-310`: process examples include `/os/processes[1]` and `/os/processes[42]/threads`.
- `object_oriented_vfs_spec.md:337`: process object is `/os/processes/{pid}`.

Risk:

Path parsing, shell completion, docs, and provider lookup will disagree.

Options:

1. POSIX-like path components only: `/os/processes/42`, `/os/devices/gpu/0`.
2. Bracket selectors in paths: `/os/devices/gpu[0]`.
3. Stable object names only: `/os/devices/gpu0`.
4. Paths plus query syntax.

Preferred:

Option 1 for canonical paths. Bracket/query syntax can be shell sugar or object-query language later. Path components should remain simple names.

### L4. Event Names Should Use One Style

References:

- `object_oriented_vfs_spec.md:332-335`: events like `process_created` and `process_exited`.
- `object_oriented_vfs_spec.md:365-370`: events like `exited`, `faulted`, and `state_changed`.
- `os1-shell-language-first-draft.md:638-645`: shell events like `on_child_start`, `on_child_exit`, and `on_signal`.
- `os1-shell-language-first-draft.md:1025-1036`: process events like `on_start`, `on_exit`, and `on_state_change`.

Risk:

Interface descriptors and shell syntax will duplicate event names with and without `on_`.

Options:

1. Store event names with `on_`.
2. Store event names without `on_`; language binding adds `on_` if desired.
3. Allow both aliases.

Preferred:

Option 2. Canonical event descriptor names should be nouns or past-tense events without `on_`, such as `started`, `exited`, `state_changed`. Shell syntax may expose `on_exit` as binding sugar.

### L5. `os.observe` Is Both Property And Method

References:

- `os1-shell-language-first-draft.md:824-855`: `os.observe` is an observability top-level property/object.
- `os1-shell-language-first-draft.md:960-967`: `os.observe(target, options)` is a core method.
- `os1-shell-language-first-draft.md:1114-1123`: examples call `os.observe(os.process)` and subscribe to `os.observe.on_trace_event`.

Risk:

The shell parser and users cannot tell whether `observe` is a callable object, namespace, method, or both.

Options:

1. Make `os.observe` a callable observability object.
2. Rename property to `os.observability`.
3. Rename method to `os.observe.attach`.

Preferred:

Option 1 or 3. Prefer `os.observe` as an object with `attach(target)`, `trace_start`, and events. Then examples become `obs = os.observe.attach(os.process)`.

### L6. Standard Collection Types Duplicate `Map` And `Dictionary`

References:

- `object_oriented_vfs_spec.md:113-119`: lists both `Dictionary<K, V>` and `Map<K, V>`.
- `object_oriented_vfs_spec.md:845-850`: repeats both names.

Risk:

Two names for the same abstraction will fragment descriptors and bindings.

Options:

1. Keep both as aliases.
2. Choose `Map<K, V>`.
3. Choose `Dictionary<K, V>`.

Preferred:

Option 2. Use `Map<K, V>` as canonical; language bindings may alias to `Dictionary` where idiomatic.

### L7. Branch Catalog Is Too Detailed For The Foundational Spec

References:

- `object_oriented_vfs_spec.md:218-752`: detailed properties, methods, and events for system, kernel, processes, devices, drivers, services, memory, events, files, and directories.
- `README.md:34-42`: the current decision needed is narrower: minimal descriptor/handle contract.

Risk:

Detailed branch APIs may create accidental commitments before the handle, descriptor, type, lifecycle, and rights specs exist.

Options:

1. Keep full branch catalog as normative.
2. Move branch catalog to examples/non-normative appendix.
3. Split foundational namespace rules from branch-specific API drafts.

Preferred:

Option 3. Make VFS semantics normative first. Move branch tables into separate draft modules and mark them non-normative until type and rights specs exist.

### L8. Shell Literal And Access Syntax Is Not Yet Consistent

References:

- `os1-shell-language-first-draft.md:223-228`: records use field names with semicolon-separated nested fields.
- `os1-shell-language-first-draft.md:1197-1203`: example uses `dimension: {1024, 768}` rather than named fields.
- `os1-shell-language-first-draft.md:155-159` and `os1-shell-language-first-draft.md:1191-1193`: examples mutate nested argument properties before running a hosted object.

Risk:

Examples may harden into syntax before the language grammar and property/local-value distinction are resolved.

Options:

1. Remove syntax details from architecture docs.
2. Keep examples but label them illustrative.
3. Define a first grammar before further examples.

Preferred:

Option 2 now, option 3 later. Mark syntax as illustrative until the object ABI and property model are settled.

## Missing Foundational Specifications

These documents should exist before implementation treats the native object model as ABI:

1. `handle_descriptor_abi.md`
   - Descriptor table layout, handle/object references, rights masks, close/dup/transfer, inheritance, fd compatibility, waitability.
2. `interface_descriptor_schema.md`
   - Canonical descriptor format, type IDs, member IDs, versioning, compatibility, metadata visibility, reflection.
3. `typed_value_abi.md`
   - Primitive types, records, arrays, strings, bytes, enums, optional/result, handle transfer, out-of-line buffers, maximum sizes.
4. `rights_credentials_policy.md`
   - Credentials, capabilities, rights attenuation, namespace policy, per-interface permissions, observability tiers.
5. `vfs_namespace_semantics.md`
   - Path grammar, lookup, mount/link semantics, rename/unlink races, object identity, provider boundaries, path-to-handle rules.
6. `event_queue_subscription.md`
   - Event queues, subscription handles, filters, ordering, overflow, backpressure, cancellation, cleanup, handler failure projection.
7. `object_lifecycle_ownership.md`
   - Reference roots, strong/weak edges, hosted children, service publication, shared memory, subscriptions, process death cleanup.
8. `loader_runtime_elf_binding.md`
   - `.os.interface` profile, constructor ABI, in-process versus hosted binding, symbol visibility, relocation, loader service responsibilities.
9. `ipc_object_call_transport.md`
   - Hosted method/property calls, serialization, handle passing, failure transport, cancellation, timeouts, provider crash behavior.
10. `standard_object_types.md`
    - `Stream<T>`, `Collection<T>`, `Map<K,V>`, `MemoryObject`, `EventQueue`, `Job<T>`, `Process`, `Service`, `ClientSession`.
11. `posix_projection.md`
    - fd semantics, read/write/stat/ioctl/mmap/poll mapping, errno mapping, exec inheritance, PTY/socket projection.
12. `state_machines.md`
    - Process, thread, job, service, driver, device, stream, subscription, and mapping states and transitions.

## Canonical Shared Definitions To Establish

- Object image: loadable code/data/metadata source, not live state.
- Object instance: live object endpoint with identity, lifetime, and interface.
- Kernel object: object owned and implemented by kernel code.
- Hosted object: object endpoint implemented by an isolated process or hosted runtime.
- In-process object: userspace object loaded into caller address space; not automatically kernel-visible.
- Descriptor: process-local integer table entry at syscall edge.
- Handle: rights-bearing reference stored in a descriptor entry or transferred as an object capability.
- Interface descriptor: canonical runtime schema listing properties, methods, events, type IDs, versions, rights, and concurrency/failure metadata.
- Property: named typed member accessed by `get_property` and optionally `set_property`.
- Method: named typed operation invoked through `call`.
- Event: named typed notification delivered through subscription and event queue objects.
- Event queue: waitable object receiving event records.
- Subscription: object linking emitter, event ID/filter, and delivery queue.
- Object reference: typed transferable reference to an object, encoded as descriptor transfer/borrow.
- Namespace object: object exposing child lookup and listing.
- Provider: kernel or userspace component implementing objects behind namespace or service boundaries.
- Capability: transferable or credential-derived authority that can grant or attenuate rights.
- Credential: process identity and security labels used by policy at open/connect/load time.
- Job: waitable async operation object with result/failure/cancel semantics.
- Service: hosted object rooted by service manager policy and published for multi-client connection.

## Terminology To Normalize

- Use `descriptor` for fd-shaped per-process integer entries.
- Use `handle` for rights-bearing object references.
- Use `fd` only for POSIX compatibility semantics.
- Use `query_interface` instead of mixing `describe`, `info`, and `describe_basic`.
- Use `call` or `call_method`, not both in the same spec.
- Use `subscribe`/`unsubscribe` for kernel operations; use `on_` only as shell binding sugar.
- Use one path syntax: slash-separated components.
- Use one primitive type spelling or canonical type IDs with display-name aliases.
- Use `rights` for handle operation bits; use `permissions` for policy inputs such as file mode/ACLs; use `capabilities` for authority-bearing grants.
- Use `hosted object` only for process/runtime-backed object endpoints; use `service` for published supervised hosted objects.
- Use `object-valued property` for properties that return handles or object references.

## Candidate Abstractions To Merge Or Split

Merge:

- POSIX fd table and native handle table into one descriptor table.
- Event callback syntax and subscription handles by making callbacks runtime sugar over queue subscriptions.
- `/os` namespace and shell `os` object by making `os` a capability-limited projection onto canonical objects.
- Directory child traversal and collection traversal through a canonical namespace/collection interface.
- `Dictionary<K,V>` and `Map<K,V>` into one canonical type.

Split:

- ELF metadata transport from canonical runtime interface descriptors.
- Local language references from kernel handles.
- Program images, processes, hosted object endpoints, services, client sessions, and jobs.
- Property binding mutation from mutation of object-valued property targets.
- Credentials, permissions, capabilities, and handle rights.
- Memory objects and mappings from generic streams.
- Namespace lookup policy from object method dispatch.

## Recommended Canonical Architecture Decisions

AD-1. Descriptor table:

The syscall edge uses fd-shaped descriptor numbers. Each descriptor references a native object handle, rights mask, descriptor flags, and optional per-open state. POSIX fds are a compatibility interpretation of descriptor entries.

AD-2. Object model:

The kernel recognizes live object instances through handles. Object images are loadable sources. Hosted and in-process loading are separate profiles. Local shell/runtime objects are not kernel handles unless explicitly exported.

AD-3. Interface model:

Every object exposes a canonical interface descriptor with exactly three member categories: properties, methods, and events. Streams, collections, memory, queues, jobs, and services are standard object types, not descriptor categories.

AD-4. Properties:

Properties are typed members accessed through `get_property`. A property may support setter-backed `set_property` if declared writable. Complex state changes should be explicit methods. Object-valued properties return object references/handles.

AD-5. Methods:

Methods are invoked through one uniform `call` operation. Synchronous calls return a typed value or canonical failure. Async methods return `Job<T>` handles. Blocking, cancellation, reentrancy, and failure metadata are descriptor fields.

AD-6. Events:

Kernel event delivery is queue-based. `subscribe` attaches an event source to an event queue and returns a subscription handle. Callback syntax is shell/runtime sugar. Ordering, overflow, filtering, and cleanup are specified by event descriptors and queue policy.

AD-7. Namespace:

VFS paths resolve to handles. A path walk uses namespace/collection interfaces internally. Canonical paths use slash-separated components. `/dev`, `/proc`, and `/sys` are POSIX-style projections, not native authority roots.

AD-8. Ambient `os`:

The shell's `os` binding is a process-context object and convenience projection onto canonical handles and namespaces. It must not define a second independent OS API.

AD-9. Rights and security:

Open/connect/load uses credentials and policy to grant descriptors with attenuated rights. All operations enforce descriptor rights. File permissions and ACLs are policy inputs. Capabilities are transferable authority objects or credential claims.

AD-10. Serialization:

Runtime syscall/IPC transport uses a canonical typed value ABI. JSON/YAML may be a source/debug format for descriptors, especially in ELF metadata, but not the kernel's long-term runtime ABI.

AD-11. ELF:

`.os.interface` is a metadata transport. It lowers to canonical descriptors. Raw symbols and calling conventions are internal to the hosting process/runtime. The kernel must not call arbitrary user symbols or expose direct memory properties as v1 baseline.

AD-12. Lifetime:

Object lifetime is governed by explicit roots and strong/weak edges. Descriptor entries hold strong references. Namespaces and service managers can root objects. Hosted children are rooted by the owning process unless published/detached. Subscription references must be weak where cycles would otherwise arise.

AD-13. POSIX:

POSIX compatibility maps onto native descriptors and standard object types. The native model remains authoritative. Early implementation should keep fd-shaped ABI compatibility without forcing every object to behave as a POSIX file internally.

AD-14. Concurrency:

Interface descriptors declare operation-level blocking, atomicity, ordering, thread-safety, and reentrancy guarantees. Defaults should be conservative: no implicit reentrancy, no global stream cursor sharing unless declared, no synchronous cross-process callbacks.

## Open Design Questions

1. What is the exact descriptor table entry layout for v1?
2. Which descriptor flags exist on day one: close-on-exec, inheritable, nonblocking, append, event-readable, transferable?
3. Are descriptor IDs generation-tagged to avoid stale-handle bugs?
4. What are the v1 rights bits and how are per-method/property/event rights encoded?
5. What is the canonical typed value envelope and maximum inline payload size?
6. Are interface member IDs stable per interface, globally stable, or name-hashed?
7. Does the kernel parse ELF interface metadata or does a trusted loader service register canonical descriptors?
8. What is the constructor/entrypoint ABI for hosted objects?
9. How does `os.load` differ from `os.spawn` in process creation, ownership, and lifetime?
10. Is `os.connect` always service-manager mediated?
11. Does `file.data` return a per-open stream view, or does the file handle implement `Stream<byte>` directly?
12. What is the exact event queue overflow policy?
13. Can event subscriptions keep emitters alive?
14. Are synchronous progress events during a method call required for v1?
15. What is the v1 process state machine, and how does it relate to jobs?
16. What observability data is public, same-user, debug, admin, or kernel-only?
17. Is direct property assignment allowed for remote hosted objects, and what atomicity does it promise?
18. Are VFS paths UTF-8, byte strings, or structured components?
19. How are mounts, object links, and provider boundaries represented?
20. Is `fork` an eventual POSIX target or is compatibility based on `spawn`/`exec` only?

## Suggested Document Merge And Restructure Plan

1. Create `00-principles-and-terms.md`.
   - Merge the stable principles from `native_object_kernel_contract.md`, the README, and `GOALS.md`.
   - Define canonical terms and explicitly mark non-goals.
2. Create `01-handle-descriptor-abi.md`.
   - This should be the next implementation-facing document.
   - Pull in handle semantics from the native contract and POSIX fd compatibility requirements from README/VFS.
3. Create `02-interface-descriptor-and-value-abi.md`.
   - Merge property/method/event descriptor rules, type/value ABI, IDs, versioning, and failure envelopes.
4. Create `03-event-queue-and-subscription.md`.
   - Replace callback-as-ABI language in the ELF and shell drafts with queue semantics plus runtime sugar.
5. Create `04-vfs-namespace-semantics.md`.
   - Keep path-to-handle semantics, provider model, namespace traversal, and POSIX projection boundaries.
   - Move branch tables to appendices or separate non-normative branch specs.
6. Create `05-loader-runtime-and-elf-metadata.md`.
   - Reframe `elf_interface_spec.md` as a transport/profile spec that lowers to canonical descriptors.
7. Create `06-security-rights-and-credentials.md`.
   - Unify rights, permissions, capabilities, users, observability tiers, and delegation.
8. Create `07-lifecycle-and-state-machines.md`.
   - Define ownership roots, hosted child rules, publication, subscriptions, services, jobs, and state transitions.
9. Keep `os1-shell-language-first-draft.md` as non-normative until documents 1-8 are stable.
   - Rewrite it later as language binding over the canonical object API.
10. Replace `README.md` with a status/index that says which specs are normative, draft, or illustrative.

## Priority-Ordered Next Steps

1. Decide descriptor table versus handle table terminology and write the v1 descriptor/handle ABI.
2. Choose the v1 property rule: getter plus optional setter-backed writable properties, with complex mutations as methods.
3. Choose the v1 event model: queue-based kernel ABI with subscription descriptors and runtime callback sugar.
4. Define the minimal typed value ABI needed for `open`, `query_interface`, `get_property`, `set_property`, `call`, `subscribe`, and `wait`.
5. Define v1 rights bits, descriptor inheritance, duplication, transfer, and close semantics.
6. Define canonical interface member IDs and versioning rules.
7. Specify VFS path grammar and namespace traversal using a canonical `Namespace`/`Collection<ObjectRef>` interface.
8. Define stream per-open state and how files expose `Stream<byte>` without breaking POSIX fd offsets.
9. Specify hosted-object loader flow: image lookup, metadata parsing, constructor, descriptor registration, method IPC, failure propagation.
10. Specify observability privilege tiers before adding more object introspection endpoints.
11. Move branch-specific VFS APIs into non-normative appendices until the type and rights specs exist.
12. Rewrite shell examples to use the canonical object API and label remaining syntax as illustrative.

## Final Recommendation

The architecture should converge on one source of truth: a native object ABI built from fd-shaped descriptors, rights-bearing handles, canonical interface descriptors, typed value envelopes, queue-based events, and explicit lifecycle roots. VFS, ELF metadata, services, POSIX compatibility, and the shell should all lower to that substrate rather than defining parallel object systems.

The first implementation slice should not attempt the full `/os` branch catalog or shell language. It should implement the descriptor table, object references, basic rights, `query_interface`, `get_property`, `set_property`, `call`, `close`, `dup`, and a minimal namespace/file object. That is the smallest slice that preserves the project's core philosophy while preventing accidental POSIX-only or shell-specific architecture from setting the ABI.
