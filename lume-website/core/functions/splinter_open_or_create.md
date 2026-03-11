---
title: splinter_open_or_create()
parent: "Function Reference"
nav_order: 4
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

# `splinter_open_or_create()` : Opens Or Creates A Splinter Memory Store

Sometimes it can be advantageous to automatically fall back to creating a store
if it doesn't yet exist when you try to open it.

Imagine if you used Splinter for logging and formatted persistent stores as
`YYYY-MM-DDZ.spl`, you'd want to create it if it failed to open, because that
means it's time for a new day's file. That's exactly the kind of shortcut the
function was written to address.

If the _reverse_ of that sounds just like what you need,
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/), is also
helpful.

## Prototype & Rationale:

The function `splinter_open_or_create()` is prototyped in `splinter.h` as:

```c
int splinter_open_or_create(const char *name_or_path, size_t slots, size_t max_value_sz);
```

When using **persistent** mode, `name_or_path` represents a physical file
location. When using standard (in-memory) mode, `name_or_path` represents the
name of the anonymous file handle (as seen via `/dev/shm/`).

And, if open fails, the same values required for
[`splinter_create()`](/core/functions/splinter-create/) are needed: `slots`
indicate how many slots will share the value region and `max_value_size`
indicates how much each one can hold.

## Return Value:

`splinter_create_or_open()` will return 0 on success, and -1 on failure. The
underlying syscalls _may_ set `errno`.

## Example:

The following example attempts to create a store with 1k rows that hold 4k worth
of value each:

```c
#include "splinter.h"

int i = -1;

i = splinter_open_or_create("debug_bus", 1024, 1024*1024);
if (i != 0) {
    exit(127);
}
```

### CLI & Tooling Example Uses:

To see the `splinter_open_or_create()` in-use within the tools that ship with
Splinter, see the following source files:

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
[`splinter_open()`](/core/functions/splinter-open/),
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
[`splinter_open_numa()`](/core/functions/splinter-open-numa/) and
[`splinter_close()`](/core/functions/splinter-close/).
