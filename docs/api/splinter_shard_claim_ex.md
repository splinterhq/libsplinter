---
title: "splinter_shard_claim_ex"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_shard_claim_ex` Splinter API Reference

The purpose of `splinter_shard_claim_ex` is to claim a shard bid with an explicit pid and claimed_at, letting a supervisor register a bid on behalf of another process or letting tests build deterministic election scenarios.

### Forward Declaration & Use

`int splinter_shard_claim_ex(uint32_t shard_id, uint32_t pid, uint8_t intent, uint8_t priority, uint64_t duration_tsc, uint64_t claimed_at)` `<splinter.h>`

```
splinter_shard_claim_ex(0x5F1A, 4242, SPL_INTENT_SEQUENTIAL, 10, 100000, splinter_now());
```

### Return & Rationale

**Return Behavior:**
Returns the same codes as `splinter_shard_claim`: 0 on success, -1 if the table is full (errno=ENOSPC), or -2 on bad args / no store / shard_id==0.

**Errno Behavior:**
A return of -1 sets `errno = ENOSPC` when the bid table is full (as with `splinter_shard_claim`).

**Rationale (Or None):**
Production shards should prefer `splinter_shard_claim`; this variant exists for supervisors and for tests that need to construct election scenarios without sleeping.

### See Also

**Relevant Symbols (Or None):**
[splinter_shard_claim](splinter_shard_claim.md), [splinter_shard_rebid](splinter_shard_rebid.md), [splinter_shard_election](splinter_shard_election.md)
