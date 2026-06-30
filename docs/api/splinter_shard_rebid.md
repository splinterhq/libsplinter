---
title: "splinter_shard_rebid | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_rebid` Splinter API Reference

The purpose of `splinter_shard_rebid` is to refresh an existing claim's window, updating claimed_at to `splinter_now()` and optionally changing intent, priority, or duration.

### Forward Declaration & Use

`int splinter_shard_rebid(uint32_t shard_id, uint8_t intent, uint8_t priority, uint64_t duration_tsc)` `<splinter.h>`

```
/* Need more time? Re-bid rather than hold sovereignty by squatting. */
splinter_shard_rebid(0x5F1A, SPL_INTENT_WILLNEED, 200, 50000);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 if `shard_id` holds no slot, or -2 on bad args.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
This is the fairness re-bid: a shard that needs more time must re-bid rather than hold, so sovereignty is never retained by squatting.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_claim](splinter_shard_claim.md), [splinter_shard_release](splinter_shard_release.md), [splinter_shard_election](splinter_shard_election.md)
