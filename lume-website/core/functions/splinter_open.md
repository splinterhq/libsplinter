---
title: splinter_open
parent: "Function Reference"
nav_order: 2
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

# `splinter_open()` : Opens A Splinter Memory Store

When opening a store that has already been provisioned, the shortest code path
is through `splinter_open()`. While all methods practically end with the store
being opened, workflow dictates that certain attempts should fail, rather than
blindly create. You should use `splinter_open()` when you're sure the store has
already been provisioned by
[`splinter_create()`](/core/functions/splinter-create/), and want a failure
condition if it can't be found.

If you want to automatically fall-back to provisioning stores if they have not
been created yet, use
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/) or
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
depending on your need.

## Prototype & Rationale:

The function `splinter_open()` is prototyped in `splinter.h` as:

```c
int splinter_open(const char *name_or_path);
```

When using **persistent** mode, `name_or_path` represents a physical file
location. When using standard (in-memory) mode, `name_or_path` represents the
name of the anonymous file handle (as seen via `/dev/shm/`).

## Return Value:

`splinter_open` will return 0 on success, and -1 on failure. Does not set
`errno`; it is either able to open and map the region or is not.

## Example:

The following example attempts to open a memory store named `debug_bus`, and
exits with a status of 127 if it can't.

```c
#include "splinter.h"

int i = -1;

i = splinter_open("debug_bus");
if (i != 0) {
    exit(127);
}
```

### CLI & Tooling Example Uses:

To see the `splinter_open()` in-use within the tools that ship with Splinter,
see the following source files:

| File                      | What `splinter_open()` Does There                                                                     |
| ------------------------- | ----------------------------------------------------------------------------------------------------- |
| `splinter.c`              | Main core library implementation                                                                      |
| `splinference.cpp`        | Opens stores to watch for embeddings that need generated.                                             |
| `splinter_cli_main.c`     | Used in cases where a user specifies a store to use in advance.                                       |
| `splinter_cli_cmd_use.c`  | Used when a user selects a new store to open.                                                         |
| `splinter_cli_cmd_init.c` | Used to re-attach to whatever store you were attached to prior to initializing a new store with init. |

### See Also:

The following functions are also relevant:
[`splinter_create()`](/core/functions/splinter-create/),
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/),
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
[`splinter_open_numa()`](/core/functions/splinter-open-numa/) and
[`splinter_close()`](/core/functions/splinter-close/).
