---
title: "splinter_get_slot_snapshot"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_slot_snapshot` Splinter API Reference

The purpose of `splinter_get_slot_snapshot` is to copy the atomic slot header for a key into a non-atomic client-side `splinter_slot_snapshot_t` for safe inspection of that slot's metadata.

### Forward Declaration & Use

`int splinter_get_slot_snapshot(const char *key, splinter_slot_snapshot_t *snapshot)` `<splinter.h>`

```
splinter_slot_snapshot_t snap = {0};
if (splinter_get_slot_snapshot("mykey", &snap) == 0) {
    printf("epoch=%lu len=%u type=0x%x\n",
           snap.epoch, snap.val_len, snap.type_flag);
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_get_header_snapshot](splinter_get_header_snapshot.md), [splinter_get_epoch](splinter_get_epoch.md), [splinter_get](splinter_get.md)
