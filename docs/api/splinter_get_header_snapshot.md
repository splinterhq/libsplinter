---
title: "splinter_get_header_snapshot"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_header_snapshot` Splinter API Reference

The purpose of `splinter_get_header_snapshot` is to copy the store's current atomic header into a non-atomic client-side `splinter_header_snapshot_t` for safe inspection of store geometry and metadata.

### Forward Declaration & Use

`int splinter_get_header_snapshot(splinter_header_snapshot_t *snapshot)` `<splinter.h>`

```
splinter_header_snapshot_t snap = {0};
if (splinter_get_header_snapshot(&snap) == 0) {
    printf("slots=%u max_val_sz=%u epoch=%lu\n",
           snap.slots, snap.max_val_sz, snap.epoch);
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
The live header is atomic and shared across processes; copying a snapshot lets a client read geometry (slots, max value size) without risk, since static geometry cannot change at runtime.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_slot_snapshot](splinter_get_slot_snapshot.md), [splinter_config_snapshot](splinter_config_snapshot.md)
