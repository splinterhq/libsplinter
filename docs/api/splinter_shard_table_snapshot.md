---
title: "splinter_shard_table_snapshot | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_table_snapshot` Splinter API Reference

The purpose of `splinter_shard_table_snapshot` is to copy a non-atomic snapshot of every bid slot for inspection or audit.

### Forward Declaration & Use

`int splinter_shard_table_snapshot(struct splinter_shard_bid_snapshot *out, size_t max)` `<splinter.h>`

```
struct splinter_shard_bid_snapshot bids[SPLINTER_MAX_SHARDS];
int n = splinter_shard_table_snapshot(bids, SPLINTER_MAX_SHARDS);
for (int i = 0; i < n; i++) { /* inspect bids[i] */ }
```

### Return & Rationale

**Return Behavior:**
Returns the number of records copied, or -2 on bad args / no store. `out` must hold at least `SPLINTER_MAX_SHARDS` records; `max` is capped at `SPLINTER_MAX_SHARDS`.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_election](splinter_shard_election.md), [splinter_shard_claim](splinter_shard_claim.md)
