---
title: "splinter_watch_unregister | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_watch_unregister` Splinter API Reference

The purpose of `splinter_watch_unregister` is to unregister interest in a key's group signal.

### Forward Declaration & Use

`int splinter_watch_unregister(const char *key, uint8_t group_id)` `<splinter.h>`

```
splinter_watch_unregister("status", 3);
```

### Return & Rationale

**Return Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_watch_register](splinter_watch_register.md), [splinter_watch_label_register](splinter_watch_label_register.md)
