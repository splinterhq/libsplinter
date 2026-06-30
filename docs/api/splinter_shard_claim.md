---
title: "splinter_shard_claim | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_claim` Splinter API Reference

The purpose of `splinter_shard_claim` is to claim a shard bid slot and declare a memory-advisement intent, CAS-claiming the first empty slot (or refreshing the caller's existing one) and stamping pid and claim time.

### Forward Declaration & Use

`int splinter_shard_claim(uint32_t shard_id, uint8_t intent, uint8_t priority, uint64_t duration_tsc)` `<splinter.h>`

```
/* Latency-sensitive: short window, high priority, re-bid often. */
splinter_shard_claim(0x5F1A, SPL_INTENT_WILLNEED, 200, 50000);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 if the table is full (errno=ENOSPC), or -2 on bad args / no store / shard_id==0. `shard_id` must be a caller-chosen non-zero identifier; `priority` is 0-255 (higher wins); `duration_tsc` is a window in `splinter_now()` ticks.

**Errno Behavior:**
A return of -1 sets `errno = ENOSPC` when the 32-slot bid table is full.

**Rationale (Or None):**
On a fresh claim the descriptive fields are published (release order) after the shard_id CAS makes the slot visible; a racing election may briefly treat the bid as SPL_INTENT_NONE / priority 0 for at most one election, which is acceptable because advisement is a hint.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_claim_ex](splinter_shard_claim_ex.md), [splinter_shard_rebid](splinter_shard_rebid.md), [splinter_shard_release](splinter_shard_release.md), [splinter_madvise](splinter_madvise.md)
