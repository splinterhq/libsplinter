---
title: "list"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `list` CLI User's Reference

The purpose of `list` is to list the keys in the currently selected store, sorted so the most recently updated keys appear first.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[pattern]` | No | A pattern to filter key names (matched with the built-in grawk engine). |
| `[max_lines]` | No | Documented in the usage string as a maximum line count. |

### Example Uses

**Console:**
```
splinter_debug # list
Name                                         Epoch  Len    Type
greeting                                     12     5      vartext
```

**Shell:**
```
$ splinterctl list greet
```

### Additional Information And Rationale

**Additional Info (Or None):**
The listing shows each key's name, epoch, value length, and type. Keys are sorted by epoch in descending order so the most-updated keys are at the top.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[get](splinterctl_get.md), [head](splinterctl_head.md), [export](splinterctl_export.md)
