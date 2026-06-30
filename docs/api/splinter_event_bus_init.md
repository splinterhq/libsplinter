---
title: "splinter_event_bus_init | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_event_bus_init` Splinter API Reference

The purpose of `splinter_event_bus_init` is to initialize the event bus from the owner process: it creates an eventfd, stores the owner PID and fd in the shared header, and arms the process-local write fd used by subsequent writes.

### Forward Declaration & Use

`int splinter_event_bus_init(void)` `<splinter.h>`

```
/* Call once, from the process that creates or governs the bus. */
if (splinter_event_bus_init() != 0) {
    perror("splinter_event_bus_init");
}
```

### Return & Rationale

**Return Behavior:**
Returns 0 on success and -1 on failure (errno set).

**Errno Behavior:**
On failure `errno` is set (by the underlying eventfd/syscall machinery).

**Rationale (Or None):**
Call once per store lifetime, from the process that creates or governs the bus, so that writes can advance an eventfd other processes block on.

### See Also

**Relevant Symbols (Or None):**
[splinter_event_bus_open](splinter_event_bus_open.md), [splinter_event_bus_wait](splinter_event_bus_wait.md), [splinter_event_bus_get_dirty](splinter_event_bus_get_dirty.md)
