---
title: "splinter_set_named_type | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_named_type` Splinter API Reference

The purpose of `splinter_set_named_type` is to declare (name) a slot's type by applying a Splinter type bitmask to the key.

### Forward Declaration & Use

`int splinter_set_named_type(const char *key, uint16_t mask)` `<splinter.h>`

```
/* Mark "counter" as a big unsigned integer slot. */
splinter_set_named_type("counter", SPL_SLOT_TYPE_BIGUINT);
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on error (sets errno).

**Errno Behavior:**
On error the function sets `errno` (the specific value is not enumerated in the header).

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_integer_op](splinter_integer_op.md), [splinter_get_slot_snapshot](splinter_get_slot_snapshot.md)
