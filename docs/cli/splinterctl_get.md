---
title: "get | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `get` CLI User's Reference

The purpose of `get` is to retrieve and print the value of a key in the store.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key whose value to retrieve. |

### Example Uses

**Console:**
```
splinter_debug # get greeting
hello
```

**Shell:**
```
$ splinterctl get greeting
hello
```

### Additional Information And Rationale

**Additional Info (Or None):**
Output is type-aware: keys typed `BIGUINT` are printed as an unsigned number, otherwise the value is printed as a string. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[set](splinterctl_set.md), [head](splinterctl_head.md), [type](splinterctl_type.md)
