---
title: "splinter_slot_usr_snapshot"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_slot_usr_snapshot` Splinter API Reference

The purpose of `splinter_slot_usr_snapshot` is to get a snapshot of a slot's user-defined flags.

### Forward Declaration & Use

`uint16_t splinter_slot_usr_snapshot(struct splinter_slot *slot)` `<splinter.h>`

```
/* slot points at a mapped slot structure. */
uint16_t flags = splinter_slot_usr_snapshot(slot);
```

### Return & Rationale

**Return Behavior:**
Returns the slot's user flags as a `uint16_t`.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_slot_usr_set](splinter_slot_usr_set.md), [splinter_slot_usr_clear](splinter_slot_usr_clear.md), [splinter_slot_usr_test](splinter_slot_usr_test.md)
