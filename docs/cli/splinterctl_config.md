---
title: "config | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `config` CLI User's Reference

The purpose of `config` is to display the current Splinter bus settings or set a bus feature flag.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| (none) | No | With no arguments, displays the current bus settings (magic, version, slots, alignment, max_val_sz, epoch, auto_scrub). |
| `[feature_flag] [flag_value]` | No | Sets a feature flag to a value. The help lists the `mop` flag with values `0`, `1`, or `2`. |

### Example Uses

**Console:**
```
splinter_debug # config
magic:       1397049428
version:     4
slots:       1024
alignment:   64
max_val_sz:  4096
epoch:       12
auto_scrub : 1
```

**Shell:**
```
$ splinterctl config
```

### Additional Information And Rationale

**Additional Info (Or None):**
The built-in help advertises a `mop` feature flag (`0` off, `1` hybrid, `2` boil), but the current argument parser matches the token `av` for that setting and routes it to `splinter_set_mop()`; the value is validated to the range 0–2.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[head](splinterctl_head.md), [caps](splinterctl_caps.md)
