---
title: "splinter_bump_slot | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_bump_slot` Splinter API Reference

The purpose of `splinter_bump_slot` is to advance a slot's epoch without otherwise modifying it, which is useful with labeling to make automation fire.

### Forward Declaration & Use

`int splinter_bump_slot(const char *key)` `<splinter.h>`

```
/* After adjusting bloom labels, bump so watchers observe the change. */
splinter_bump_slot("mykey");
```

### Return & Rationale

**Return Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
Advancing the epoch republishes a slot so watchers and the event bus observe a change, even when the value itself was not rewritten (for example after only labels were adjusted). It is classified as a HIGH-risk operation in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_epoch](splinter_get_epoch.md), [splinter_set_label](splinter_set_label.md), [splinter_pulse_keygroup](splinter_pulse_keygroup.md)
