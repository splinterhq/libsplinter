---
title: "splinter_client_unset_tandem"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_client_unset_tandem` Splinter API Reference

The purpose of `splinter_client_unset_tandem` is to delete a base key and its known ordered (tandem) members.

### Forward Declaration & Use

`void splinter_client_unset_tandem(const char *base_key, uint8_t orders)` `<splinter.h>`

```
splinter_client_unset_tandem("car", 2); /* removes car and its 2 orders */
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void).

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_client_set_tandem](splinter_client_set_tandem.md), [splinter_unset](splinter_unset.md)
