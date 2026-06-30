---
title: "splinter_shard_election | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_election` Splinter API Reference

The purpose of `splinter_shard_election` is to run the read-only election scan and return the current sovereign shard, optionally reporting the winning intent.

### Forward Declaration & Use

`uint32_t splinter_shard_election(uint8_t *out_intent)` `<splinter.h>`

```
uint8_t intent = SPL_INTENT_NONE;
uint32_t sovereign = splinter_shard_election(&intent);
if (sovereign == my_shard_id) { /* I won — issue advice */ }
```

### Return & Rationale

**Return Behavior:**
Returns the winning shard_id, or 0 if there is no current sovereign. `out_intent` is optional and receives the winning bid's intent (or SPL_INTENT_NONE).

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
The highest-priority unexpired bid wins; ties break by earliest claimed_at, then lowest pid. A DONTNEED bid is skipped as a winner while any unexpired WILLNEED or SEQUENTIAL bid exists. The scan is O(32) and deterministic from static bid data, so every process computes the same sovereign with no arbiter.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_is_sovereign](splinter_shard_is_sovereign.md), [splinter_shard_table_snapshot](splinter_shard_table_snapshot.md), [splinter_madvise](splinter_madvise.md)
