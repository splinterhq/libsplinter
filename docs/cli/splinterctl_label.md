---
title: "label"
parent: "Splinter CLI Reference"
date: 2026-06-30
updated: 2026-06-30
---

## `label` CLI User's Reference

The purpose of `label` is to apply a label (a bit pattern) to a key's 64-bit Bloom filter.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key>` | Yes | The key to label. |
| `<label_name>` | Yes | A label name defined in `~/.splinterrc`, or a hex/numeric mask. |

### Example Uses

**Console:**
```
splinter_debug # label job:7 WAITING
Label 'WAITING' (0x1) applied to 'job:7'.
```

**Shell:**
```
$ splinterctl label job:7 0x1
```

### Additional Information And Rationale

**Additional Info (Or None):**
The label name is resolved against `~/.splinterrc` first; if not found, it is parsed as a numeric mask. A mask of `0` or an unparseable value is rejected.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[bind](splinterctl_bind.md), [math](splinterctl_math.md), [watch](splinterctl_watch.md)
