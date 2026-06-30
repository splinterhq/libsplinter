---
title: "bind | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `bind` CLI User's Reference

The purpose of `bind` is to map a semantic Bloom label to a signal group (0-63), so that when a key's label matches the mask the signal group is pulsed.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `[label_name \| mask]` | Yes | A label name from `~/.splinterrc`, or a numeric mask. |
| `<group_id>` | Yes | The signal group (0-63) to bind the label to. |
| `-b, --bloom` | No | Explicitly indicate this is a Bloom-to-Group binding (currently the only mode). |
| `-h, --help` | No | Show usage. |

### Example Uses

**Console:**
```
splinter_debug # bind WAITING 5
```

**Shell:**
```
$ splinterctl bind 0x1 5 --bloom
```

### Additional Information And Rationale

**Additional Info (Or None):**
Bloom-to-group binding is currently the only supported mode. The label/mask is resolved against `~/.splinterrc` the same way as the `label` command.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[label](splinterctl_label.md), [watch](splinterctl_watch.md)
