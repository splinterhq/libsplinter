---
title: "splinter_set_as_system | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_as_system` Splinter API Reference

The purpose of `splinter_set_as_system` is to promote a key to "system" usage.

### Forward Declaration & Use

`int splinter_set_as_system(const char *key)` `<splinter.h>`

```
splinter_set_as_system("__meta__");
```

### Return & Rationale

**Return Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Classified as a HIGH-risk operation (permanent state change) in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_named_type](splinter_set_named_type.md), [splinter_set_label](splinter_set_label.md)
