---
title: "splinter_append"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_append` Splinter API Reference

The purpose of `splinter_append` is to append data to an existing key's value in place, optionally reporting the new total length.

### Forward Declaration & Use

`int splinter_append(const char *key, const void *data, size_t data_len, size_t *new_len)` `<splinter.h>`

```
size_t total = 0;
if (splinter_append("log", " more", 5, &total) == 0) {
    printf("new length: %zu\n", total);
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 if the key is not found or the append would overflow, or -2 if arguments are invalid. `new_len` may be NULL.

**Errno Behavior:**
Per the AI Primer, an append that would exceed `max_val_sz` returns -1 with `errno == EMSGSIZE` (a geometry problem, not contention — do not retry verbatim). Check before appending in a loop.

**Rationale (Or None):**
`max_val_sz` is a hard per-slot ceiling; an append cannot grow a value beyond it.

### See Also

**Relevant Symbols (Or None):**
[splinter_set](splinter_set.md), [splinter_get_header_snapshot](splinter_get_header_snapshot.md)
