---
title: "splinter_enumerate_matches"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_enumerate_matches` Splinter API Reference

The purpose of `splinter_enumerate_matches` is to iterate through all slots whose Bloom mask matches a given mask, invoking a callback for each match.

### Forward Declaration & Use

`void splinter_enumerate_matches(uint64_t mask, void (*callback)(const char *key, uint64_t version, void *data), void *user_data)` `<splinter.h>`

```
void on_match(const char *key, uint64_t version, void *data) {
    printf("match: %s (epoch %lu)\n", key, version);
}
splinter_enumerate_matches(0x1ULL, on_match, NULL);
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void). It visits only slots where `(slot->bloom & mask) == mask`.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
This is an O(slots) scan, not an O(1) index — it is intended for batch operations and semantic routing, not hot-path queries.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_label](splinter_set_label.md), [splinter_list](splinter_list.md)
