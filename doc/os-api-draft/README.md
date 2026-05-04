# OS API Drafts

These documents are design exploration, not implemented kernel contracts.

The source tree remains the authority for current behavior. As of 2026-05-05,
the implemented syscall surface is the small ABI in
[`src/uapi/os1/syscall_numbers.h`](../../src/uapi/os1/syscall_numbers.h):
console `read`/`write`, process control for initrd-backed programs, and
`observe`.

The kernel does not yet implement:

- a native object table
- a per-process descriptor or handle table
- a VFS object model
- sockets
- PTYs
- argv/envp process startup
- filesystem-backed `exec`
- credentials or permission checks

## Read Order

1. [`native_object_kernel_contract.md`](native_object_kernel_contract.md) for the
   broad native-object direction being explored.
2. [`object_oriented_vfs_spec.md`](object_oriented_vfs_spec.md) for one possible
   filesystem/object namespace shape.
3. [`elf_interface_spec.md`](elf_interface_spec.md) for intended user-program ABI
   growth beyond the current static `ET_EXEC` loader.
4. [`os1-shell-language-first-draft.md`](os1-shell-language-first-draft.md) for
   shell-language ideas that depend on the lower-level process, descriptor, and
   filesystem decisions.

## Current Decision Needed

The next implementation decision is intentionally narrower than these drafts:
define the minimal per-process descriptor or handle contract that can support
console fd 0/1/2, VFS files, devices, PTYs, and future sockets.

That decision should preserve compatibility with fd-shaped user APIs at the
syscall edge unless there is a deliberate reason to diverge. The internal kernel
object model can still be native.
