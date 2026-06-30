---
title: "splinter_create_or_open | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_create_or_open` Splinter API Reference

The purpose of `splinter_create_or_open` is to create a new splinter store with the given geometry, or open it if it already exists.

### Forward Declaration & Use

`int splinter_create_or_open(const char *name_or_path, size_t slots, size_t max_value_sz)` `<splinter.h>`

```
/* Create "mystore" with 1024 x 4096, or open it if it already exists. */
if (splinter_create_or_open("mystore", 1024, 4096) != 0) {
    perror("splinter_create_or_open");
    return 1;
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_open_or_create](splinter_open_or_create.md), [splinter_create](splinter_create.md), [splinter_open](splinter_open.md)
