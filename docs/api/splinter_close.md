---
title: "splinter_close | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_close` Splinter API Reference

The purpose of `splinter_close` is to close the splinter store and unmap its shared memory region.

### Forward Declaration & Use

`void splinter_close(void)` `<splinter.h>`

```
splinter_open("mystore");
/* ... work with the store ... */
splinter_close();
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
[splinter_open](splinter_open.md), [splinter_create](splinter_create.md)
