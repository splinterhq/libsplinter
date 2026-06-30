---
title: "splinter_config_test | Splinter API"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_config_test` Splinter API Reference

The purpose of `splinter_config_test` is to test a bitmask against a Splinter bus header's configuration flags.

### Forward Declaration & Use

`int splinter_config_test(struct splinter_header *hdr, uint8_t mask)` `<splinter.h>`

```
/* hdr points at the mapped store header. */
if (splinter_config_test(hdr, SPL_SYS_AUTO_SCRUB)) {
    /* the auto-scrub flag is set */
}
```

### Return & Rationale

**Return Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Errno Behavior:**
CONFIDENCE_TOO_LOW_FOR_GENERATION

**Rationale (Or None):**
None

### See Also

**Relevant Symbols (Or None):**
[splinter_config_set](splinter_config_set.md), [splinter_config_clear](splinter_config_clear.md), [splinter_config_snapshot](splinter_config_snapshot.md)
