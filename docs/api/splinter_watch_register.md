---
title: "splinter_watch_register | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_watch_register` Splinter API Reference

The purpose of `splinter_watch_register` is to register interest in a key's group signal so that writes to it pulse the given signal group.

### Forward Declaration & Use

`int splinter_watch_register(const char *key, uint8_t group_id)` `<splinter.h>`

```
splinter_watch_register("status", 3); /* subscribe "status" to group 3 */
```

### Return & Rationale

**Return Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Splinter has 64 signal groups (0-63). Subscribing a key to a group lets readers poll a single counter and then scan for changed epochs rather than sweeping the whole store.

### See Also

**Relevant Symbols (Or None):**
[splinter_watch_unregister](splinter_watch_unregister.md), [splinter_watch_label_register](splinter_watch_label_register.md), [splinter_get_signal_count](splinter_get_signal_count.md)
