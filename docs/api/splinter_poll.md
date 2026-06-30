---
title: "splinter_poll"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_poll` Splinter API Reference

The purpose of `splinter_poll` is to wait up to `timeout_ms` milliseconds for a key's value to change.

### Forward Declaration & Use

`int splinter_poll(const char *key, uint64_t timeout_ms)` `<splinter.h>`

```
int rc = splinter_poll("status", 100); /* wait up to 100 ms */
if (rc == 0) {
    /* value changed — read it */
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 if the value changed, or -1 on timeout or if the key does not exist.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Polling a single key is the simplest change-detection floor; for blocking, kernel-assisted waits across the whole store, prefer the event bus.

### See Also

**Relevant Symbols (Or None):**
[splinter_get_epoch](splinter_get_epoch.md), [splinter_event_bus_wait](splinter_event_bus_wait.md), [splinter_get_signal_count](splinter_get_signal_count.md)
