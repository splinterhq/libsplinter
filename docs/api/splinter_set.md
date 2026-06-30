---
title: "splinter_set"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set` Splinter API Reference

The purpose of `splinter_set` is to set or update the value of a key in the store, writing `len` bytes from `val` (which must not exceed `max_val_sz`).

### Forward Declaration & Use

`int splinter_set(const char *key, const void *val, size_t len)` `<splinter.h>`

```
const char *msg = "hello";
if (splinter_set("greeting", msg, strlen(msg)) != 0) {
    perror("splinter_set");
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure (for example, when the store is full).

**Errno Behavior:**
Per the AI Primer, a return of -1 with `errno == EAGAIN` means the slot is momentarily contested by a writer and the call should be retried; `ENOSPC` indicates the store is full (there is no eviction).

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_get](splinter_get.md), [splinter_append](splinter_append.md), [splinter_unset](splinter_unset.md)
