---
title: "splinter_open_numa"
parent: "API Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `splinter_open_numa` Splinter API Reference

The purpose of `splinter_open_numa` is to open the Splinter bus and bind it to a specific NUMA node so the VALUES arena and slot pages stay local to the target socket's memory controller.

### Forward Declaration & Use

`void* splinter_open_numa(const char *name, int target_node)` `<splinter.h>`

```
/* Requires a build with SPLINTER_NUMA_AFFINITY defined. */
void *store = splinter_open_numa("mystore", 0); /* pin to NUMA node 0 */
```

### Return & Rationale

**Return Behavior:**
*None.*

**Errno Behavior:**
*None.*

**Rationale (Or None):**
Binding the mapping to one NUMA node keeps all pages for the arena and slots local to a single socket's memory controller, preserving cache and memory locality.

### See Also

**Relevant Symbols (Or None):**
[splinter_open](splinter_open.md), [splinter_create](splinter_create.md), [splinter_close](splinter_close.md)
