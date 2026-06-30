---
title: "splinter_slot_usr_set | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_slot_usr_set` Splinter API Reference

The purpose of `splinter_slot_usr_set` is to set a user-defined flag bitmask on a slot.

### Forward Declaration & Use

`void splinter_slot_usr_set(struct splinter_slot *slot, uint16_t mask)` `<splinter.h>`

```
/* slot points at a mapped slot structure. */
splinter_slot_usr_set(slot, SPL_FUSR1);
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void).

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_slot_usr_clear](splinter_slot_usr_clear.md), [splinter_slot_usr_test](splinter_slot_usr_test.md), [splinter_slot_usr_snapshot](splinter_slot_usr_snapshot.md)
