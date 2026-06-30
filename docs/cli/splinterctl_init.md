---
title: "init | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `init` CLI User's Reference

The purpose of `init` is to create a Splinter store with default or specific geometry.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[store_name]` | No | Name of the store to create. Defaults to the compiled-in `DEFAULT_BUS` (`splinter_debug`). |
| `-s, --slots <N>` | No | Maximum number of slots. Defaults to `DEFAULT_SLOTS` (1024). |
| `-l, --length <N>` | No | Maximum value length. Defaults to `DEFAULT_VAL_MAXLEN` (4096). |
| `-h, --help` | No | Show usage. |

### Example Uses

**Console:**
```
splinter_debug # init mystore --slots 4096 --length 8192
```

**Shell:**
```
$ splinterctl init mystore -s 4096 -l 8192
```

### Additional Information And Rationale

**Additional Info (Or None):**
If arguments are omitted, the compiled-in defaults are used. The built-in usage string refers to the value-length option as `--maxlen`, while the parser registers it as `-l` / `--length`.

**Rationale (Or None):**
Splinter has static geometry: slot count and max value size are fixed at creation, so they are chosen here.

### See Also

**Related Commands (Or None):**
[use](splinterctl_use.md), [config](splinterctl_config.md)
