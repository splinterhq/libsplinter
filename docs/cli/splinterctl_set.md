---
title: "set"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `set` CLI User's Reference

The purpose of `set` is to set the value of a key in the store.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key to write. |
| `"<value>"` | Yes | The value to store. Keys without spaces do not need to be quoted. |

### Example Uses

**Console:**
```
splinter_debug # set greeting "hello world"
```

**Shell:**
```
$ splinterctl set greeting "hello world"
```

### Additional Information And Rationale

**Additional Info (Or None):**
The value length is bounded at read time to 4096 bytes by the CLI. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[get](splinterctl_get.md), [append](splinterctl_append.md), [unset](splinterctl_unset.md)
