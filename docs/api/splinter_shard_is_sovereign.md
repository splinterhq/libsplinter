---
title: "splinter_shard_is_sovereign | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_is_sovereign` Splinter API Reference

The purpose of `splinter_shard_is_sovereign` is a convenience check for whether a given shard_id is the current election sovereign.

### Forward Declaration & Use

`int splinter_shard_is_sovereign(uint32_t shard_id)` `<splinter.h>`

```
if (splinter_shard_is_sovereign(0x5F1A) == 1) {
    /* this shard currently holds sovereignty */
}
```

### Return & Rationale

**Return Behavior:**
Returns 1 if sovereign, 0 if not (including unknown/expired), or -2 if no store is mapped.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_election](splinter_shard_election.md), [splinter_madvise](splinter_madvise.md)
