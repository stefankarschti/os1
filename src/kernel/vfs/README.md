# Kernel VFS

This directory is reserved for the future filesystem namespace layer: mount tables, path lookup, vnode/inode handles, file descriptors, and filesystem-backed `exec`.

Concrete filesystem parsers belong under `fs/`; storage devices and block caches belong under `storage/`.
