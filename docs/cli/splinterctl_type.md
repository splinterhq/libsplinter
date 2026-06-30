---
title: "type"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `type` CLI User's Reference

The purpose of `type` is to display or set the named slot type for a key.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key to inspect or retype. |
| `<type>` | No | When given, sets the slot's type. One of: `void`, `bigint`, `biguint`, `json`, `binary`, `img`, `audio`, `vartext`. |

### Example Uses

**Console:**
```
splinter_debug # type counter biguint
```

**Shell:**
```
$ splinterctl type counter
biguint:counter
```

### Additional Information And Rationale

**Additional Info (Or None):**
With only a key, the current type is printed as `<type>:<key>`. With a key and a type, the slot's named type is set. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[get](splinterctl_get.md), [head](splinterctl_head.md), [math](splinterctl_math.md)
