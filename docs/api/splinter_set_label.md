---
title: "splinter_set_label"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_set_label` Splinter API Reference

The purpose of `splinter_set_label` is to atomically OR a label mask into a slot's 64-bit Bloom filter.

### Forward Declaration & Use

`int splinter_set_label(const char *key, uint64_t mask)` `<splinter.h>`

```
splinter_set_label("job:7", 0x1ULL); /* mark with label bit 0 */
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 if the key is not found.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Bloom labels are persistent until explicitly cleared and drive semantic routing; governance observes the bloom directly. Labels are classified as HIGH-risk (permanent label state change) in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_unset_label](splinter_unset_label.md), [splinter_enumerate_matches](splinter_enumerate_matches.md), [splinter_watch_label_register](splinter_watch_label_register.md)
