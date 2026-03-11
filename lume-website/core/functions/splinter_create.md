---
title: splinter_create()
parent: "Function Reference"
nav_order: 1
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

# `splinter_create()` : Creates A Splinter Memory Store

The `splinter_create()` function creates a mapped splinter store to the geometry
specified by the caller. Splinter stores are fixed-size (area) divided up into
_slots_ that can each occupy up to a certain _length_ of a value arena; they can
be created in-memory or on disk, depending on which library your client links
against.

`splinter_create()` behaves like
[`splinter_open()`](/core/functions/splinter-open/) in how it returns (and in
how it leaves you with an attached, active store). If you don't _immediately_
need a store created with `splinter_create()`, call `splinter_close()`. Note
that `splinter_create()` will fail if a store by the given name already exists.

For practical convenience, you might prefer to use
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/) or
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
depending on your need and workflow consistency.

## Prototype & Rationale:

The function `splinter_create()` is prototyped in `splinter.h` as:

```c
int splinter_create(const char *name_or_path, size_t slots, size_t max_value_sz);
```

When using **persistent** mode, `name_or_path` represents a physical file
location. When using standard (in-memory) mode, `name_or_path` represents the
name of the anonymous file handle (as seen via `/dev/shm/`).

`slots` are the number of key->value rows the store will have, and
`max_value_sz` is the maximum size any slot's share of the global value region
can be.

## Return Value:

`splinter_create()` will return 0 on success, and -1 on failure. It doesn't
itself set `errno`, but underlying system calls may (e.g file exists).

## Example:

The following example attempts to create a Splinter memory store named
`debug_bus`, with 1024 key->value rows, where each slot's share of the value
region can't exceed 4k:

```c
#include "splinter.h"

int i = -1;

i = splinter_create("debug_bus", 1024, 4*1024);
if (i != 0) {
    exit(127);
}
```

### CLI & Tooling Example Uses:

To see the `splinter_create()` in-use within the tools that ship with Splinter,
see the following source files:

| File                      | What `splinter_create()` Does There  |
| ------------------------- | ------------------------------------ |
| `splinter.c`              | Main core library implementation     |
| `splinter_cli_cmd_init.c` | Used to create new stores on-demand. |

### See Also:

The following functions are also relevant:
[`splinter_open()`](/core/functions/splinter-open/),
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/),
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
[`splinter_open_numa()`](/core/functions/splinter-open-numa/) and
[`splinter_close()`](/core/functions/splinter-close/).
