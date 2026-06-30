---
title: "unset"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `unset` CLI User's Reference

The purpose of `unset` is to delete a key (and optionally its tandem keys) from the store.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `-r` | No | Recursively remove tandem keys (`key.1`, `key.2`, ...). |
| `<key_name>` | Yes | The key to delete. |

### Example Uses

**Console:**
```
splinter_debug # unset -r car
3 bytes deleted.
```

**Shell:**
```
$ splinterctl unset greeting
```

### Additional Information And Rationale

**Additional Info (Or None):**
Prints the total number of bytes deleted. With `-r`, it iterates tandem suffixes (using the order accessor) until a key is not found. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[set](splinterctl_set.md), [orders](splinterctl_orders.md)
