---
title: "splinter_pulse_keygroup"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_pulse_keygroup` Splinter API Reference

The purpose of `splinter_pulse_keygroup` is to pulse a key's signal group, identified by one of its member keys.

### Forward Declaration & Use

`int splinter_pulse_keygroup(const char *key)` `<splinter.h>`

```
splinter_pulse_keygroup("status");
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success, -2 on system failure, or -1 if the key is not found.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Classified as a HIGH-risk operation (signal propagation) in the AI Primer.

### See Also

**Relevant Symbols (Or None):**
[splinter_pulse_watchers](splinter_pulse_watchers.md), [splinter_get_signal_count](splinter_get_signal_count.md), [splinter_watch_register](splinter_watch_register.md)
