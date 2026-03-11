---
title: splinter_open_numa()
parent: "Function Reference"
nav_order: 5
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

# `splinter_open_numa()` : Opens A Store With Numa Affinity/Bias

The `splinter_open_numa()` is the way to open a splinter store with NUMA pinning
to take advantage of same-lane performance increases, which can be quite
substantial on modern hardware.

This ensures all memory pages for the values arena and slots stay local to the
target socket's memory controller.

To use `splinter_open_numa()`, you just have to pass the ID of the NUMA node
after the name of the store, and ensure any additional processes that attach
also declare the same affinity.

## Prototype & Rationale:

The function `splinter_open()` is prototyped in `splinter.h` as:

```c
int splinter_open(const char *name_or_path, int target_node);
```

You should **not** use this function in persistent mode.

## Return Value:

`splinter_open_numa()` will return 0 on success, and -1 on failure. Does not set
`errno`; it is either able to open and map the region or is not.

## Example:

The following example attempts to open a memory store named `debug_bus`, and
exits with a status of 127 if it can't.

```c
#include "splinter.h"
#define TARGET_NUMA_NODE 1

int i = -1;

i = splinter_open_numa("debug_bus", TARGET_NUMA_NODE);
if (i != 0) {
    exit(127);
}
```

### CLI & Tooling Example Uses:

**None.**

### See Also:

The following functions are also relevant:
[`splinter_create()`](/core/functions/splinter-create/),
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/),
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
[`splinter_open()`](/core/functions/splinter-open/) and
[`splinter_close()`](/core/functions/splinter-close/).
