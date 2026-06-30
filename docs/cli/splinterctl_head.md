---
title: "head | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `head` CLI User's Reference

The purpose of `head` is to display the metadata of a key in the store (without its value).

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key whose metadata to display. |

### Example Uses

**Console:**
```
splinter_debug # head greeting
```

**Shell:**
```
$ splinterctl head greeting
```

### Additional Information And Rationale

**Additional Info (Or None):**
If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[get](splinterctl_get.md), [type](splinterctl_type.md), [list](splinterctl_list.md)
