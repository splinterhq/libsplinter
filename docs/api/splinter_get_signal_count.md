---
title: "splinter_get_signal_count"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_get_signal_count` Splinter API Reference

The purpose of `splinter_get_signal_count` is to safely retrieve the current pulse count for a signal group, which serves as a watcher's heartbeat check.

### Forward Declaration & Use

`uint64_t splinter_get_signal_count(uint8_t group_id)` `<splinter.h>`

```
uint64_t last = splinter_get_signal_count(3);
/* ... later ... */
if (splinter_get_signal_count(3) != last) {
    /* group 3 pulsed — scan for changed slots */
}
```

### Return & Rationale

**Return Behavior:**
Returns the 64-bit pulse count for `group_id` (0-63), or 0 if the group id is invalid.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Readers poll this counter; when it changes they scan for modified epochs. This is the polling floor of the pub/sub system, beneath the kernel-assisted event bus.

### See Also

**Relevant Symbols (Or None):**
[splinter_watch_register](splinter_watch_register.md), [splinter_pulse_keygroup](splinter_pulse_keygroup.md), [splinter_event_bus_wait](splinter_event_bus_wait.md)
