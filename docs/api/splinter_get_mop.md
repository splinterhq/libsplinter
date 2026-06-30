---
title: "splinter_get_mop | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_mop` Splinter API Reference

The purpose of `splinter_get_mop` is to report the store's current mop (auto-scrub) mode.

### Forward Declaration & Use

`int splinter_get_mop(void)` `<splinter.h>`

```
int mode = splinter_get_mop();
/* 0 = off, 1 = hybrid, 2 = full boil, -2 = no store */
```

### Return & Rationale

**Return Behavior:**
Returns 0 (off), 1 (hybrid), or 2 (full boil); returns -2 when no store is mapped.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_set_mop](splinter_set_mop.md), [splinter_purge](splinter_purge.md)
