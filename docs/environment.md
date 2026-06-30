---
title: "Environment Variables"
nav_order: 4
date: 2026-06-30
updated: 2026-06-30
---

# Environment Variables

Splinter honors a small number of environment variables. They affect both library
(API) use and the CLI tools, since the CLI creates and addresses stores through the
same library calls.

## `SPLINTER_DEFAULT_UMASK`

**Controls the file permissions of newly created stores.** This affects both API
and CLI use.

A new store — the POSIX shared-memory object in the default build, or the backing
file in persistent mode — is created with mode `0666` masked by the process umask.
By default that means it inherits the shell's umask, which on most GNU systems
(`umask 022`) leaves the store **readable by other local users** (`0644`). For a
shared-memory substrate this can be an unwelcome surprise.

Splinter deliberately does **not** change that default, because the common case is
local and/or automated use that may already rely on the inherited umask. Instead,
set `SPLINTER_DEFAULT_UMASK` to an **octal** umask value to override the permissions
*at creation time only*:

| Value | Resulting store mode | Meaning |
| --- | --- | --- |
| (unset) | `0666 & ~shell_umask` (often `0644`) | default — inherits the shell umask |
| `077` | `0600` | private to the owner |
| `027` | `0640` | owner read/write, group read |
| `022` | `0644` | owner read/write, others read |

```sh
# CLI: create a private store
SPLINTER_DEFAULT_UMASK=077 splinterctl init mystore

# API: same effect, applied around splinter_create()
SPLINTER_DEFAULT_UMASK=077 ./my_program
```

Notes and limits:

- It is applied as the process umask **only for the duration of store creation**,
  then the previous umask is restored.
- It takes effect only when a store is **created**. It does not change the
  permissions of a store that already exists — use `chmod` (or the usual tools)
  to adjust those afterward.
- An unset, empty, or unparseable value is ignored, and creation falls back to the
  inherited umask. The value must be a valid octal mask in the range `0`–`0777`.
- Applies to persistent mode as well as the default shared-memory mode.

See [`splinter_create`](api/splinter_create.md) (API) and
[`init`](cli/splinterctl_init.md) (CLI) for the creation paths this governs.

## `SPLINTER_NS_PREFIX`

**Prepends a namespace prefix to keys** in the CLI's key-addressed commands
(`get`, `set`, `head`, `append`, `unset`, `bump`, `retrain`, `watch`, `type`, and
others). When set, the prefix is prepended to the key name before the operation, so
a value of `tenantA:` turns `get widget` into a read of `tenantA:widget`.

It can also be set for the current `splinterctl` / `splinter_cli` session with the
`--prefix` / `-p` program option, which exports it into the environment.

```sh
SPLINTER_NS_PREFIX="tenantA:" splinterctl get widget   # reads tenantA:widget
splinterctl --prefix "tenantA:" get widget             # equivalent
```

This is a CLI-side convenience; the library's own functions take whole key strings
as given.
