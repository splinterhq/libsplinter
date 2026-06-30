---
title: "splinter_unset_label"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_unset_label` Splinter API Reference

The purpose of `splinter_unset_label` is to atomically remove a previously applied Bloom label mask from a slot.

### Forward Declaration & Use

`int splinter_unset_label(const char *key, uint64_t mask)` `<splinter.h>`

```
splinter_unset_label("job:7", 0x1ULL); /* clear label bit 0 */
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and a negative value on failure.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Labels persist until explicitly cleared with this function; `splinter_unset` also clears them as part of slot destruction.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_label](splinter_set_label.md), [splinter_unset](splinter_unset.md)
