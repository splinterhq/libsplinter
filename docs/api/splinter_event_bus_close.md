---
title: "splinter_event_bus_close | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_event_bus_close` Splinter API Reference

The purpose of `splinter_event_bus_close` is to close a fd previously obtained from `splinter_event_bus_open`.

### Forward Declaration & Use

`void splinter_event_bus_close(int fd)` `<splinter.h>`

```
int fd = splinter_event_bus_open();
/* ... */
splinter_event_bus_close(fd);
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
[splinter_event_bus_open](splinter_event_bus_open.md), [splinter_event_bus_wait](splinter_event_bus_wait.md)
