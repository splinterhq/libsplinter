---
title: "splinter_shard_release | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_release` Splinter API Reference

The purpose of `splinter_shard_release` is to voluntarily release (yield) the caller's bid slot, zeroing `shard_id` last after clearing the other fields.

### Forward Declaration & Use

`int splinter_shard_release(uint32_t shard_id)` `<splinter.h>`

```
splinter_shard_release(0x5F1A);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 if `shard_id` holds no slot, or -2 on bad args / no store.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Zeroing `shard_id` last (after the other fields) keeps the slot's transition to "empty" well-defined for a concurrent election scan.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_claim](splinter_shard_claim.md), [splinter_shard_rebid](splinter_shard_rebid.md), [splinter_shard_election](splinter_shard_election.md)
