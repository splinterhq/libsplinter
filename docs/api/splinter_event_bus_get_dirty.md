---
title: "splinter_event_bus_get_dirty | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_event_bus_get_dirty` Splinter API Reference

The purpose of `splinter_event_bus_get_dirty` is to copy a snapshot of the dirty-slot bitmask into caller-supplied storage so a watcher can enumerate only the slots that changed.

### Forward Declaration & Use

`void splinter_event_bus_get_dirty(uint64_t *out, size_t words)` `<splinter.h>`

```
uint64_t dirty[SPLINTER_EVENT_BUS_MASK_WORDS];
splinter_event_bus_get_dirty(dirty, SPLINTER_EVENT_BUS_MASK_WORDS);
/* bit i in word w => physical slot index (w*64 + i) */
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void). `out` must hold at least `words` `uint64_t` values; `words` is capped at `SPLINTER_EVENT_BUS_MASK_WORDS`.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Each set bit i in word w marks physical slot (w*64 + i) as written since the bus was initialized. Bits are never cleared by the library, so callers diff the snapshot against their own saved copy to find newly-dirtied slots.

### See Also

**Relevant Symbols (Or None):**
[splinter_event_bus_wait](splinter_event_bus_wait.md), [splinter_event_bus_init](splinter_event_bus_init.md)
