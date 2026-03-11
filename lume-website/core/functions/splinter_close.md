---
title: splinter_close()
parent: "Function Reference"
nav_order: 6
---

# `splinter_close()` : Closes A Splinter Memory Store

The `splinter_close()` function closes the current process' view of a shared
Splinter store. When all processes have closed their view of the store, the
kernel handles housekeeping.

In shared-memory configuration (not persistent), programs only need to call
`splinter_close()` if they intend to open a different store. Smaller stores are
likely to persist in `/dev/shm` until reboot, even if all processes have closed
their view of it, just because the kernel won't reclaim them until it needs to
(like any other paging).

However, if using persistent mode, you should call `splinter_close()` as soon as
you can know (in code) when you won't need access to the store any longer. It
helps to put it in your clean-up / exit handlers, just so hanging processes
don't keep large chunks of contiguous memory busy any longer than absolutely
necessary.

Basically, if persisting, treat `splinter_close()` like `close()`, but it takes
no arguments, and you only have to call it once at the end.

## Prototype & Rationale:

The function `splinter_close()` is prototyped in `splinter.h` as:

```
int splinter_close(void);
```

If / when Splinter allows for multiple file-backed stores to be open
simultaneously, `splinter_close()` will accept a file descriptor as an argument.

## Return Value:

`splinter_close()` returns void and does not set `errno`.

## Example:

The following closes a store at the end of work:

```c
#include "splinter.h"

int i = -1;

i = splinter_open("debug_bus");
if (i != 0) {
    exit(127);
}
splinter_close();
// any splinter calls (except to open) after this will fail
```

### CLI & Tooling Example Uses:

To see the `splinter_close()` in-use within the tools that ship with Splinter,
see the following source files:

| File         | What `splinter_close()` Does There |
| ------------ | ---------------------------------- |
| `splinter.c` | Main core library implementation   |

Due to the simplicity of the function, no further examples are warranted.

### See Also:

The following functions are also relevant:
[`splinter_create()`](/core/functions/splinter-create/),
[`splinter_open_or_create()`](/core/functions/splinter-open-or-create/),
[`splinter_create_or_open()`](/core/functions/splinter-create-or-open/),
[`splinter_open_numa()`](/core/functions/splinter-open-numa/) and
[`splinter_open()`](/core/functions/splinter-open/).
