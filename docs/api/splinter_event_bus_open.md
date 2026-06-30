---
title: "splinter_event_bus_open | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_event_bus_open` Splinter API Reference

The purpose of `splinter_event_bus_open` is to open a process-local read fd to the owner's eventfd by reading the owner PID and fd from the shared header and opening `/proc/<owner_pid>/fd/<owner_fd>`.

### Forward Declaration & Use

`int splinter_event_bus_open(void)` `<splinter.h>`

```
int fd = splinter_event_bus_open();
if (fd < 0) { perror("splinter_event_bus_open"); }
/* ... wait on fd ... */
splinter_event_bus_close(fd);
```

### Return & Rationale

**Return Behavior:**
Returns a valid fd on success (which the caller must pass to `splinter_event_bus_close`), or -1 on failure (errno set).

**Errno Behavior:**
On failure `errno` is set (by the underlying open/syscall machinery).

**Rationale (Or None):**
Opening `/proc/<owner_pid>/fd/<owner_fd>` yields a local descriptor to the same kernel eventfd object, working both in the owner process and in any other process that has the store mapped.

### See Also

**Relevant Symbols (Or None):**
[splinter_event_bus_init](splinter_event_bus_init.md), [splinter_event_bus_wait](splinter_event_bus_wait.md), [splinter_event_bus_close](splinter_event_bus_close.md)
