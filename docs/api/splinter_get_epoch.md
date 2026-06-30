---
title: "splinter_get_epoch"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_epoch` Splinter API Reference

The purpose of `splinter_get_epoch` is to get the current epoch of a specific slot, the cornerstone of the seqlock consistency protocol.

### Forward Declaration & Use

`uint64_t splinter_get_epoch(const char *key)` `<splinter.h>`

```
uint64_t e1 = splinter_get_epoch("mykey");
if (e1 & 1) { /* writer active — retry */ }
/* ... read ... */
uint64_t e2 = splinter_get_epoch("mykey");
if (e1 != e2) { /* torn read — retry */ }
```

### Return & Rationale

**Return Behavior:**
Returns the 64-bit epoch, or 0 if the key is not found. An even epoch means the slot is stable; an odd epoch means a writer is active.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
The epoch is the only thing standing between a reader and silent cross-process data corruption: check it before and after a read, and treat any change (forward or backward) as a torn read. Note that `splinter_retrain_slot` can drive an epoch backward.

### See Also

**Relevant Symbols (Or None):**
[splinter_bump_slot](splinter_bump_slot.md), [splinter_get_raw_ptr](splinter_get_raw_ptr.md), [splinter_retrain_slot](splinter_retrain_slot.md)
