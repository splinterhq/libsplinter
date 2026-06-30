---
title: "hist | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `hist` CLI User's Reference

The purpose of `hist` is to show the command history, optionally filtered by a pattern.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[pattern]` | No | A pattern to filter history entries (matched with the built-in grawk engine). |

### Example Uses

**Console:**
```
splinter_debug # hist set
1: set greeting hello
```

**Shell:**
```
$ splinterctl hist
```

### Additional Information And Rationale

**Additional Info (Or None):**
History entries are printed with a 1-based index. Passing more than one argument prints the usage message.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[help](splinterctl_help.md)
