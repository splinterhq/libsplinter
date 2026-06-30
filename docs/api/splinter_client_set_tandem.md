---
title: "splinter_client_set_tandem"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_client_set_tandem` Splinter API Reference

The purpose of `splinter_client_set_tandem` is to write multiple ordered "tandem" values for a base key, managing the ordered-set naming convention on the caller's behalf.

### Forward Declaration & Use

`int splinter_client_set_tandem(const char *base_key, const void **vals, const size_t *lens, uint8_t orders)` `<splinter.h>`

```
const void *vals[2] = { "v1", "v2" };
size_t lens[2] = { 2, 2 };
splinter_client_set_tandem("car", vals, lens, 2); /* writes car, car.1 ... */
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -1 on failure, or -2 if underlying basic I/O calls fail.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
The helper manages the tandem-key naming convention (using the order accessor) so callers can store related ordered values — for example coupled vectors — under one base key.

### See Also

**Relevant Symbols (Or None):**
[splinter_client_unset_tandem](splinter_client_unset_tandem.md), [splinter_set](splinter_set.md)
