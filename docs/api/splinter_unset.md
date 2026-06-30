---
title: "splinter_unset | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_unset` Splinter API Reference

The purpose of `splinter_unset` is to delete a key: it atomically zeroes the slot hash to mark it available, then zeroes the used key and value regions and resets the slot.

### Forward Declaration & Use

`int splinter_unset(const char *key)` `<splinter.h>`

```
int freed = splinter_unset("greeting");
if (freed >= 0)
    printf("freed %d bytes\n", freed);
```

### Return & Rationale

**Return Behavior:**
Returns the length of the value deleted, -1 if the key is not found, or -2 on a NULL key/store.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Zeroing the hash first marks the slot writable in a single atomic operation; clearing the key and value regions afterward prevents stale data from lingering. This is classified as a DESTRUCTIVE operation in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_set](splinter_set.md), [splinter_client_unset_tandem](splinter_client_unset_tandem.md), [splinter_unset_label](splinter_unset_label.md)
