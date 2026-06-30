---
title: "splinter_now"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_now` Splinter API Reference

The purpose of `splinter_now` is to read the platform's 64-bit cycle counter, used as a demarcation point for measuring elapsed time (e.g. jitter in timestamp backfill and shard bid windows).

### Forward Declaration & Use

`inline uint64_t splinter_now(void)` `<splinter.h>`

```
uint64_t waypoint = splinter_now();
splinter_set("foo", value, len);
time_t t = time(NULL);
uint64_t now = splinter_now();
splinter_set_slot_time("foo", SPL_TIME_CTIME, t, now - waypoint);
```

### Return & Rationale

**Return Behavior:**
Returns the current 64-bit cycle counter value (`uint64_t`). On x86 it uses `rdtsc`; on aarch64 the virtual counter; on arm the 64-bit physical counter; otherwise it falls back to `clock_gettime(CLOCK_MONOTONIC_RAW)` in nanoseconds.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Accessing a wall clock is not something that can be done reasonably inside a seqlock, so cycle-counter waypoints let callers backfill ctime/atime stamps with a correction offset for the syscall cost.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_slot_time](splinter_set_slot_time.md), [splinter_shard_claim](splinter_shard_claim.md)
