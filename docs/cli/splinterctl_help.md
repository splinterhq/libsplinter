---
title: "help | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `help` CLI User's Reference

The purpose of `help` is to provide help for the CLI's commands and features.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| (none) | No | Lists available commands and how to show additional help. |
| `<module_name>` | No | Shows help for a particular command/module. |
| `ext <module_name>` | No | Shows the extended help display for a command (not all commands have one). |

### Example Uses

**Console:**
```
splinter_debug # help ext help
```

**Shell:**
```
$ splinterctl help get
```

### Additional Information And Rationale

**Additional Info (Or None):**
Not all commands have extended help displays. Problems with help coverage can be reported on GitHub.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[caps](splinterctl_caps.md)
