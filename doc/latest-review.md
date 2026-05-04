# Last Review

This file is a stable pointer to the most recent code-grounded project review. Update it whenever a new dated review lands.

## Current

- [2026-05-03 review](2026-05-03-review.md) — source-grounded review after the 2026-04-30 driver/device/platform implementation pass landed (driver registry, IRQ vector allocator, MSI-X/MSI/INTx fallback, DMA buffers, shared virtio transport, BlockDevice v2 with interrupt-driven `virtio-blk` reads + writes, HPET/LAPIC timer migration, `virtio-net`, xHCI with HID boot keyboard, AML/`_PRT`/`_PS0`/`_PS3` power management), after the 2026-05-03 documentation cleanup pass that re-aligned README/GOALS/ARCHITECTURE/REFERENCES with the substrate, and after the 2026-05-03 multi-sector `virtio-blk` pass that lifted the `sector_count == 1` restriction up to a bounded 4 KiB per request and chunked the sync wrappers by `max_sectors_per_request`. Closes the BlockDevice v2, common virtio transport, MSI/MSI-X, timer-migration, AML, graduation-list, `/bin/ascii`-smoke, syscall-numbers.h-drift, and single-sector-`virtio-blk` findings from prior reviews; carries forward the native-interface decision, argv/envp, and the `BootInfo` capacity caps.

## Previous

- [2026-04-29 review (revision 2)](2026-04-29-review-2.md) — source-grounded review after the SYSCALL/SYSRET migration, typed wait state, kernel event ring, SMP synchronization contract, host-test harness, and Limine shim restructure landed (PRs #15-#19). Closes the host-test, SMP-vocabulary, event-ring, syscall-fast-path, wait-state, and shim-split findings; carries forward the native-interface decision, BlockDevice v2, common virtio transport, MSI/MSI-X, argv/envp, and `/bin/ascii` smoke gaps.
- [2026-04-29 review](2026-04-29-review.md) — first 2026-04-29 review, written before PRs #15-#19. Surfaced the native-interface decision and listed the near-term items most of which closed the same day.
- [2026-04-28 review (revision 3)](2026-04-28-review-3.md) — source-grounded review after the boot-envelope and documentation updates. Reassesses earlier findings, marks resolved issues as historical, and refocuses the roadmap on storage contracts, host tests, virtio/MSI, SMP discipline, and documentation drift.
- [2026-04-28 review (revision 2)](2026-04-28-review-2.md) — independent re-read of the same tree as revision 1. Reinforces the substrate-hardening conclusion; adds structural findings around storage, MSI/MSI-X, event logging, tests, and wait-state representation.
- [2026-04-28 review (revision 1)](2026-04-28-review-1.md) — first 2026-04-28 review.
- [2026-04-27 review](2026-04-27-review.md) — first review after the source reorganization.
- [2026-04-24 review](2026-04-24-review.md) — historical (path-stale after the reorganization).
- [2026-04-23 review](2026-04-23-review.md) — historical.
- [2026-04-21 review](2026-04-21-review.md) — historical.
- [2026-04-19 review](2026-04-19-review.md) — historical.
