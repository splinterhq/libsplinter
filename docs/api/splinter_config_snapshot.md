---
title: "splinter_config_snapshot"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_config_snapshot` Splinter API Reference

The purpose of `splinter_config_snapshot` is to snapshot a Splinter bus header's configuration flags byte.

### Forward Declaration & Use

`uint8_t splinter_config_snapshot(struct splinter_header *hdr)` `<splinter.h>`

```
/* hdr points at the mapped store header. */
uint8_t flags = splinter_config_snapshot(hdr);
```

### Return & Rationale

**Return Behavior:**
Returns the bus configuration flags as a `uint8_t`.

**Errno Behavior:**
*None.*

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_config_set](splinter_config_set.md), [splinter_config_clear](splinter_config_clear.md), [splinter_config_test](splinter_config_test.md)
