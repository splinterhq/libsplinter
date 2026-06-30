---
title: "splinter_open_or_create | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_open_or_create` Splinter API Reference

The purpose of `splinter_open_or_create` is to open an existing splinter store, or create it with the given geometry if it does not yet exist.

### Forward Declaration & Use

`int splinter_open_or_create(const char *name_or_path, size_t slots, size_t max_value_sz)` `<splinter.h>`

```
/* Open "mystore" if present, otherwise create it with 1024 x 4096. */
if (splinter_open_or_create("mystore", 1024, 4096) != 0) {
    perror("splinter_open_or_create");
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
[splinter_create_or_open](splinter_create_or_open.md), [splinter_open](splinter_open.md), [splinter_create](splinter_create.md)
