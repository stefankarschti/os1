# Kernel PTY Layer

This directory is reserved for pseudo-terminal support. It should appear as real code when sessions, remote login, or multiple user-facing terminal endpoints need a stream abstraction above the current fixed kernel terminals.

Hardware keyboard input belongs under `drivers/input`; logical terminal behavior belongs one level up in `console/`.
