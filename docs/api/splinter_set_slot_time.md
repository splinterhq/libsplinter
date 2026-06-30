---
title: "splinter_set_slot_time | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_slot_time` Splinter API Reference

The purpose of `splinter_set_slot_time` is to update a slot's ctime or atime with a client-supplied timestamp, subtracting an offset to correct for the update-after-write delay.

### Forward Declaration & Use

`int splinter_set_slot_time(const char *key, unsigned short mode, uint64_t epoch, size_t offset)` `<splinter.h>`

```
uint64_t waypoint = splinter_now();
splinter_set("foo", value, len);
time_t t = time(NULL);
uint64_t now = splinter_now();
splinter_set_slot_time("foo", SPL_TIME_CTIME, t, now - waypoint);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, or -1/-2 on error (sets errno). `mode` is `SPL_TIME_CTIME` or `SPL_TIME_ATIME`.

**Errno Behavior:**
On error the function sets `errno` (the specific value is not enumerated in the header).

**Rationale (Or None):**
Timestamps are optional and must be set by the client because a wall-clock syscall cannot be performed inside the seqlock; the `offset` subtracts the measured syscall cost so the recorded time is closer to the actual write.

### See Also

**Relevant Symbols (Or None):**
[splinter_now](splinter_now.md), [splinter_get_slot_snapshot](splinter_get_slot_snapshot.md)
