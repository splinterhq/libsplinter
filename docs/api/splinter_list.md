---
title: "splinter_list | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_list` Splinter API Reference

The purpose of `splinter_list` is to list all keys currently in the store, filling `out_keys` with pointers to the keys and reporting how many were found through `out_count`.

### Forward Declaration & Use

`int splinter_list(char **out_keys, size_t max_keys, size_t *out_count)` `<splinter.h>`

```
splinter_header_snapshot_t hdr = {0};
splinter_get_header_snapshot(&hdr);
char **keys = calloc(hdr.slots, sizeof(char *));
size_t count = 0;
if (splinter_list(keys, hdr.slots, &count) == 0) {
    for (size_t i = 0; i < count; i++)
        puts(keys[i]);
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure. Writes at most `max_keys` key pointers and stores the count found in `out_count`.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Sizing `out_keys` to the store's slot count (from a header snapshot) guarantees room for every possible key.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_header_snapshot](splinter_get_header_snapshot.md), [splinter_enumerate_matches](splinter_enumerate_matches.md)
