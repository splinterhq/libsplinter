---
title: "splinter_open | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_open` Splinter API Reference

The purpose of `splinter_open` is to open an existing splinter store by shared-memory name or filesystem path.

### Forward Declaration & Use

`int splinter_open(const char *name_or_path)` `<splinter.h>`

```
if (splinter_open("mystore") != 0) {
    perror("splinter_open");
    return 1;
}
/* ... use the store ... */
splinter_close();
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure (for example, when the store does not exist).

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_create](splinter_create.md), [splinter_open_or_create](splinter_open_or_create.md), [splinter_close](splinter_close.md)
