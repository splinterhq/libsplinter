---
title: "splinter_slot_usr_test"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_slot_usr_test` Splinter API Reference

The purpose of `splinter_slot_usr_test` is to test a user-defined flag bitmask on a slot.

### Forward Declaration & Use

`int splinter_slot_usr_test(struct splinter_slot *slot, uint16_t mask)` `<splinter.h>`

```
/* slot points at a mapped slot structure. */
if (splinter_slot_usr_test(slot, SPL_FUSR1)) {
    /* the SPL_FUSR1 flag is set */
}
```

### Return & Rationale

**Return Behavior:**
*None.*

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_slot_usr_set](splinter_slot_usr_set.md), [splinter_slot_usr_clear](splinter_slot_usr_clear.md), [splinter_slot_usr_snapshot](splinter_slot_usr_snapshot.md)
