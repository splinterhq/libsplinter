---
parent: "API Reference"
title: "splinter_watch_label_register"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_watch_label_register` Splinter API Reference

The purpose of `splinter_watch_label_register` is to map a Bloom label (bitmask) to a signal group, so any key matching the mask pulses that group.

### Forward Declaration & Use

`int splinter_watch_label_register(uint64_t bloom_mask, uint8_t group_id)` `<splinter.h>`

```
splinter_watch_label_register(0x1ULL, 5); /* label bit 0 -> group 5 */
```

### Return & Rationale

**Return Behavior:**
*None.*

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Subscribing by label lets an entire semantic class of keys wake a single signal group together, rather than registering each key individually.

### See Also

**Relevant Symbols (Or None):**
[splinter_set_label](splinter_set_label.md), [splinter_watch_register](splinter_watch_register.md), [splinter_get_signal_count](splinter_get_signal_count.md)
