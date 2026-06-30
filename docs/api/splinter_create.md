---
title: "splinter_create | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_create` Splinter API Reference

The purpose of `splinter_create` is to create and initialize a new splinter store with a fixed geometry of `slots` key-value slots and a per-value ceiling of `max_value_sz` bytes.

### Forward Declaration & Use

`int splinter_create(const char *name_or_path, size_t slots, size_t max_value_sz)` `<splinter.h>`

```
/* Create a store named "mystore" with 1024 slots, 4096 bytes max per value. */
if (splinter_create("mystore", 1024, 4096) != 0) {
    perror("splinter_create");
    return 1;
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure (for example, when the store already exists).

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Splinter has static geometry: the slot count and maximum value size are fixed at creation and cannot be resized on a live store, so they are chosen here. The new store's file permissions follow the process umask; set the `SPLINTER_DEFAULT_UMASK` environment variable to override them at creation time — see [Environment Variables](../environment.md).

### See Also

**Relevant Symbols (Or None):**
[splinter_open](splinter_open.md), [splinter_open_or_create](splinter_open_or_create.md), [splinter_create_or_open](splinter_create_or_open.md), [splinter_close](splinter_close.md)
