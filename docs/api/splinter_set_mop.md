---
title: "splinter_set_mop | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_mop` Splinter API Reference

The purpose of `splinter_set_mop` is to control the store's "mop" (auto-scrub) mode: 0 = off, 1 = hybrid, 2 = full boil.

### Forward Declaration & Use

`int splinter_set_mop(unsigned int mode)` `<splinter.h>`

```
/* Zero the whole slot on every write for verifiable zero-contamination. */
splinter_set_mop(2);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 on an invalid mode, and -2 if something is wrong with the store.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Because slots are a fixed `max_val_sz` wide but only `val_len` bytes are live, a shorter write can leave stale trailing bytes. Hybrid (1, the default) zeroes the new length plus a 64-byte-aligned slop region so SIMD loads cannot see stale data; full boil (2) zeroes the entire slot at higher cost.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_mop](splinter_get_mop.md), [splinter_purge](splinter_purge.md), [splinter_get_raw_ptr](splinter_get_raw_ptr.md)
