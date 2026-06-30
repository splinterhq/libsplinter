---
title: "splinter_purge"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_purge` Splinter API Reference

The purpose of `splinter_purge` is to sweep every key and zero out memory past each value's length up to the allocated slot length, clearing any lingering stale data.

### Forward Declaration & Use

`void splinter_purge(void)` `<splinter.h>`

```
/* Run during a quiescent maintenance window / backfill, not under I/O load. */
splinter_purge();
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void).

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Purge does not reclaim space; it only ensures the manifold is clean. It is designed to run as part of backfill runs once I/O slamming has stopped, and is classified as a DESTRUCTIVE operation in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_mop](splinter_set_mop.md), [splinter_get_mop](splinter_get_mop.md)
