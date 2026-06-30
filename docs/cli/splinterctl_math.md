---
title: "math | Splinter CLI"
date: 2026-06-30
updated: 2026-06-30
---

## `math` CLI User's Reference

The purpose of `math` is to perform increment/decrement and bitwise operations on named `biguint` keys.

### Arguments & Switches

| Argument / Switch | Required | Description |
| --- | --- | --- |
| `<key>` | Yes | The `biguint` key to operate on. |
| `<op>` | Yes | One of `inc`, `dec`, `and`, `or`, `xor`, `not`. |
| `[value]` | For all ops except `not` | A number (hex or decimal) or a label name defined in `~/.splinterrc`. |

### Example Uses

**Console:**
```
splinter_debug # math counter inc 1
Operation 'inc' applied to 'counter' successfully.
```

**Shell:**
```
$ splinterctl math flags or 0x4
```

### Additional Information And Rationale

**Additional Info (Or None):**
`not` is unary and takes no value. Values are first matched against named labels in `~/.splinterrc`, then parsed as a number. The command reports `key is not a BIGUINT slot` on `EPROTOTYPE` and `collision detected, try again` on `EAGAIN`.

**Rationale (Or None):**
None

### See Also

**Related Commands (Or None):**
[type](splinterctl_type.md), [label](splinterctl_label.md)
