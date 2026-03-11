---
title: splinter_create_or_open
parent: "Function Reference"
nav_order: 3
metas:
  lang: en
  description: >-
    Splinter is a minimalist, lock-free key-value manifold designed for high-frequency data ingestion and retrieval across disjointed runtimes using IPC.
  keywords:
    - Vector Anti-Database
    - Shared-Memory Substrate
    - Lock-Free IPC
    - LLM Semantic Memory
    - Inter-Process Communication
  robots: true
  generator: true
---

# `splinter_create_or_open()` : Opens Or Creates A Splinter Memory Store

The `splinter_create_or_open()` function behaves identically to the
[`splinter_open_or_create()`](/core/functions/splinter_open_or_create/), except
that it will first attempt to _create_ the store with specified geometry and
open an existing only if encountered.

## Prototype & Rationale:

The function `splinter_create_or_open()` is prototyped in `splinter.h` as:

```c
int splinter_create_or_open(const char *name_or_path, size_t slots, size_t max_value_sz);
```

## Refer To Reference Implementation

Refer to [splinter_open_or_create()](/core/functions/splinter_open_or_create/)
for exhaustive details, as they're identical with the exception of the order in
which operations are attempted.

### See Also:

The following functions are also relevant:
[`splinter_create()`](/core/functions/splinter-create/),
[`splinter_open()`](/core/functions/splinter-open/),
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/),
[`splinter_open_numa()`](/core/functions/splinter-open-numa/) and
[`splinter_close()`](/core/functions/splinter-close/).
