---
title: "watch"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `watch` CLI User's Reference

The purpose of `watch` is to observe a single key, or a signal group, in the current store for changes and print updates.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name_to_watch>` | One of key or `--group` | The key to watch for value changes. |
| `--group <signal_group_id>` | One of key or `--group` | Watch a signal group (0 to `SPLINTER_MAX_GROUPS`-1) for pulses instead of a key. |
| `--oneshot` | No | Exit after one event. |
| `--help` | No | Show usage. |

### Example Uses

**Console:**
```
splinter_debug # watch status --oneshot
```

**Shell:**
```
$ splinterctl watch --group 3
```

### Additional Information And Rationale

**Additional Info (Or None):**
Pressing CTRL-] terminates a waiting watch in the terminal. Key watching polls with `splinter_poll`; group watching polls the signal-group counter (50 ms interval). If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[bind](splinterctl_bind.md), [bump](splinterctl_bump.md), [label](splinterctl_label.md)
