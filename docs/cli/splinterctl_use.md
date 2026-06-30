---
title: "use | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `use` CLI User's Reference

The purpose of `use` is to select a store (by name or path) to be the current store.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<store_name_or_path>` | Yes | The shared-memory name or filesystem path of the store to open. |

### Example Uses

**Console:**
```
splinter_debug # use mystore
use: now connected to mystore
```

**Shell:**
```
$ splinterctl use mystore
```

### Additional Information And Rationale

**Additional Info (Or None):**
`use` closes any currently open store before opening the new one. On failure it reports the connection failure and leaves the session disconnected.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[init](splinterctl_init.md), [config](splinterctl_config.md)
