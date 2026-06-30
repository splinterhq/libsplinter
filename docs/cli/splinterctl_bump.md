---
title: "bump"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `bump` CLI User's Reference

The purpose of `bump` is to advance a key's epoch by one operation (a value of 2) without otherwise changing it.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key_name>` | Yes | The key whose epoch to advance. |

### Example Uses

**Console:**
```
splinter_debug # bump job:7
```

**Shell:**
```
$ splinterctl bump job:7
```

### Additional Information And Rationale

**Additional Info (Or None):**
`bump` is designed to be used after all Bloom labels have been adjusted, so watchers and automation observe the change. If the `SPLINTER_NS_PREFIX` environment variable is set, it is prepended to the key name.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[label](splinterctl_label.md), [bind](splinterctl_bind.md), [watch](splinterctl_watch.md)
