---
title: "append | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `append` CLI User's Reference

The purpose of `append` is to append a value to an existing key in the store (without truncating it first).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key to append to. |
| `"<value_to_append>"` | Yes | The data to append. Keys without spaces do not need to be quoted. |

### Example Uses

**Console:**
```
splinter_debug # append log " more text"
```

**Shell:**
```
$ splinterctl append log " more text"
```

### Additional Information And Rationale

**Additional Info (Or None):**
The appended value length is bounded at read time to 4096 bytes by the CLI. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[set](splinterctl_set.md), [get](splinterctl_get.md)
