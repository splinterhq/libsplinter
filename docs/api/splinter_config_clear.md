---
title: "splinter_config_clear"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_config_clear` Splinter API Reference

The purpose of `splinter_config_clear` is to clear a bitmask from a Splinter bus header's configuration flags.

### Forward Declaration & Use

`void splinter_config_clear(struct splinter_header *hdr, uint8_t mask)` `<splinter.h>`

```
/* hdr points at the mapped store header. */
splinter_config_clear(hdr, SPL_SYS_AUTO_SCRUB);
```

### Return & Rationale

**Return Behavior:**
This function returns no value (void).

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_config_set](splinter_config_set.md), [splinter_config_test](splinter_config_test.md), [splinter_config_snapshot](splinter_config_snapshot.md)
