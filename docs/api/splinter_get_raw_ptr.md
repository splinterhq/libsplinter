---
title: "splinter_get_raw_ptr | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_raw_ptr` Splinter API Reference

The purpose of `splinter_get_raw_ptr` is to return a direct, read-only pointer into shared memory for a key's value, along with its length and the epoch at lookup time.

### Forward Declaration & Use

`const void *splinter_get_raw_ptr(const char *key, size_t *out_sz, uint64_t *out_epoch)` `<splinter.h>`

```
size_t sz = 0;
uint64_t e1 = 0;
const void *p = splinter_get_raw_ptr("mykey", &sz, &e1);
if (p && !(e1 & 1)) {
    /* ... consume up to sz bytes ... */
    if (splinter_get_epoch("mykey") != e1) {
        /* torn read — discard and retry */
    }
}
```

### Return & Rationale

**Return Behavior:**
Returns a `const` pointer to the value data in shared memory, or NULL if the key is not found. `out_sz` receives the value length and `out_epoch` the epoch at lookup.

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
The pointer is live: another process can change or zero the memory between receiving and dereferencing it, so it must be paired with epoch verification and never held across a yield or sleep. Bytes beyond `out_sz` may be stale (see the mop modes), since raw access bypasses `val_len` checking.

### See Also

**Relevant Symbols (Or None):**
[splinter_get](splinter_get.md), [splinter_get_epoch](splinter_get_epoch.md), [splinter_set_mop](splinter_set_mop.md)
